#include "command_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Every handler has this shape: argument text in, reply text out. */
typedef cmd_result_t (*cmd_fn_t)(const char *arg, command_state_t *s,
                                 char *reply, size_t n);

typedef struct {
    const char *name;   /* full command, or prefix if takes_arg */
    bool takes_arg;     /* text after "name " is passed as arg */
    int8_t once_bit;    /* -1 = may repeat; otherwise bit in once_mask */
    bool needs_sim;     /* only accepted in SIM mode */
    cmd_fn_t fn;
} command_def_t;

/* ------------------------------------------------------------------ */
/* Handlers                                                            */
/* ------------------------------------------------------------------ */

static cmd_result_t cmd_ping(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg; (void)s;
    snprintf(reply, n, "Pong");
    return CMD_OK;
}

static cmd_result_t cmd_sim_enable(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    s->sim_enabled = true;
    snprintf(reply, n, "ACK");
    return CMD_OK;
}

static cmd_result_t cmd_sim_activate(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    if (!s->sim_enabled) {
        snprintf(reply, n, "Simulation not enabled!");
        return CMD_ERR_NOT_ENABLED;
    }
    s->op_mode = OP_MODE_SIM;
    s->sim_pressure_valid = false;   /* wait for the first SIMP value */
    snprintf(reply, n, "Simulation Activated");
    return CMD_OK;
}

static cmd_result_t cmd_sim_disable(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    s->sim_enabled = false;
    s->sim_pressure_valid = false;
    s->op_mode = OP_MODE_FLIGHT;
    snprintf(reply, n, "ACK: Simulation Deactivated.");
    return CMD_OK;
}

static cmd_result_t cmd_simp(const char *arg, command_state_t *s, char *reply, size_t n)
{
    char *end;
    unsigned long pa = strtoul(arg, &end, 10);
    /* Reject empty/garbage text and values no barometer would give. */
    if (end == arg || *end != '\0' || pa == 0 || pa > 120000) {
        snprintf(reply, n, "ERR: bad pressure value");
        return CMD_ERR_BAD_ARG;
    }
    s->sim_pressure_pa = (uint32_t)pa;
    s->sim_pressure_valid = true;
    snprintf(reply, n, "ACK: %lu", pa);
    return CMD_OK;
}

static cmd_result_t cmd_calibrate(const char *arg, command_state_t *s, char *reply, size_t n)
{
    /* The ground station may address "all" or "pocketcube". */
    if (strcmp(arg, "all") != 0 && strcmp(arg, "pocketcube") != 0) {
        snprintf(reply, n, "ERR: unknown calibrate target");
        return CMD_ERR_BAD_ARG;
    }
    /* Immediate ACK; the calibration code sends "CAL DONE" when finished. */
    s->pending_actions |= ACTION_CALIBRATE;
    snprintf(reply, n, "ACK");
    return CMD_OK;
}

static cmd_result_t cmd_deploy_sp1(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    s->pending_actions |= ACTION_DEPLOY_SOLAR_1;
    snprintf(reply, n, "ACK: Deploying solar panel one...");
    return CMD_OK;
}

static cmd_result_t cmd_deploy_sp2(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    s->pending_actions |= ACTION_DEPLOY_SOLAR_2;
    snprintf(reply, n, "ACK: Deploying solar panel two...");
    return CMD_OK;
}

static cmd_result_t cmd_deploy_boom(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    s->pending_actions |= ACTION_DEPLOY_BOOM;
    snprintf(reply, n, "ACK: Deploying magnetometer boom...");
    return CMD_OK;
}

static cmd_result_t cmd_cam_start(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    s->pending_actions |= ACTION_CAM_START;
    snprintf(reply, n, "ACK: Starting camera recording...");
    return CMD_OK;
}

static cmd_result_t cmd_cam_rotate(const char *arg, command_state_t *s, char *reply, size_t n)
{
    char *end;
    long deg = strtol(arg, &end, 10);   /* accepts "+90", "-90", "180" */
    if (end == arg || *end != '\0' || deg < -360 || deg > 360) {
        snprintf(reply, n, "ERR: bad angle");
        return CMD_ERR_BAD_ARG;
    }
    s->cam_rotate_deg = (int16_t)deg;
    s->pending_actions |= ACTION_CAM_ROTATE;
    snprintf(reply, n, "ACK: Rotating camera angle by %+ld", deg);
    return CMD_OK;
}

static cmd_result_t cmd_cam_rec_start(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    s->pending_actions |= ACTION_CAM_RECORD_START;
    snprintf(reply, n, "ACK: Starting video recording...");
    return CMD_OK;
}

static cmd_result_t cmd_cam_rec_stop(const char *arg, command_state_t *s, char *reply, size_t n)
{
    (void)arg;
    s->pending_actions |= ACTION_CAM_RECORD_STOP;
    snprintf(reply, n, "ACK: Stopping video recording...");
    return CMD_OK;
}

static cmd_result_t cmd_cam_stab(const char *arg, command_state_t *s, char *reply, size_t n)
{
    if (strcmp(arg, "on") == 0) {
        s->cam_stabilization = true;
    } else if (strcmp(arg, "off") == 0) {
        s->cam_stabilization = false;
    } else {
        snprintf(reply, n, "ERR: use on or off");
        return CMD_ERR_BAD_ARG;
    }
    s->pending_actions |= ACTION_CAM_STAB_CHANGED;
    snprintf(reply, n, "ACK: Camera stabilization %s", arg);
    return CMD_OK;
}

/* ------------------------------------------------------------------ */
/* Command table: one line per command                                 */
/* ------------------------------------------------------------------ */

static const command_def_t COMMANDS[] = {
    /* name                        arg    once            sim    handler */
    { "ping",                      false, -1,             false, cmd_ping },
    { "sim enable",                false, -1,             false, cmd_sim_enable },
    { "sim activate",              false, -1,             false, cmd_sim_activate },
    { "sim disable",               false, -1,             false, cmd_sim_disable },
    { "simp",                      true,  -1,             true,  cmd_simp },
    { "calibrate",                 true,  -1,             false, cmd_calibrate },
    { "mec deploy solar_panel_1",  false, ONCE_SOLAR_1,   false, cmd_deploy_sp1 },
    { "mec deploy solar_panel_2",  false, ONCE_SOLAR_2,   false, cmd_deploy_sp2 },
    { "mec deploy boom",           false, ONCE_BOOM,      false, cmd_deploy_boom },
    { "cam start",                 false, ONCE_CAM_START, false, cmd_cam_start },
    { "cam rotate",                true,  -1,             false, cmd_cam_rotate },
    { "cam record start",          false, -1,             false, cmd_cam_rec_start },
    { "cam record stop",           false, -1,             false, cmd_cam_rec_stop },
    { "cam stabilization",         true,  -1,             false, cmd_cam_stab },
};

#define NUM_COMMANDS (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

/*
 * Find the table entry for cmd. For commands without an argument the text
 * must match exactly. For commands with one, the text must be the name,
 * a space, then the argument; *arg is pointed at the argument.
 */
static const command_def_t *find_command(const char *cmd, const char **arg)
{
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        const command_def_t *def = &COMMANDS[i];
        size_t len = strlen(def->name);

        if (!def->takes_arg) {
            if (strcmp(cmd, def->name) == 0) {
                *arg = "";
                return def;
            }
        } else if (strncmp(cmd, def->name, len) == 0 && cmd[len] == ' ') {
            *arg = cmd + len + 1;
            return def;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void command_state_init(command_state_t *s)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->echo, sizeof(s->echo), "None");
    s->op_mode = OP_MODE_FLIGHT;
    s->cam_stabilization = true;
}

cmd_result_t command_handle(const char *cmd, command_state_t *s,
                            char *reply, size_t reply_size)
{
    if (cmd == NULL || s == NULL || reply == NULL || reply_size == 0) {
        return CMD_ERR_BAD_ARG;
    }

    /* Every received command is counted and echoed, even rejected ones. */
    s->count++;
    snprintf(s->echo, sizeof(s->echo), "%s", cmd);
    /* Commas are CSV delimiters, so they must never appear in CMD_ECHO. */
    for (char *p = s->echo; *p != '\0'; p++) {
        if (*p == ',') {
            *p = ' ';
        }
    }

    const char *arg;
    const command_def_t *def = find_command(cmd, &arg);

    if (def == NULL) {
        snprintf(reply, reply_size, "ERR: unknown command");
        return CMD_ERR_UNKNOWN;
    }
    if (def->needs_sim && s->op_mode != OP_MODE_SIM) {
        snprintf(reply, reply_size, "ERR: SIMP received in FLIGHT mode");
        return CMD_ERR_NOT_SIM;
    }
    if (def->once_bit >= 0 && (s->once_mask & (1u << def->once_bit))) {
        snprintf(reply, reply_size, "This command can only be run once!");
        return CMD_ERR_RAN_ONCE;
    }

    cmd_result_t result = def->fn(arg, s, reply, reply_size);

    /* Only mark a run-once command as used if it actually succeeded. */
    if (result == CMD_OK && def->once_bit >= 0) {
        s->once_mask |= (uint8_t)(1u << def->once_bit);
    }
    return result;
}