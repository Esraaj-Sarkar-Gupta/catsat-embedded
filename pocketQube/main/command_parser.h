#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * PocketQube command parser.
 *
 * Commands arrive as lowercase text (e.g. "sim enable", "mec deploy boom",
 * "simp 101325"). command_handle() updates the command state and writes a
 * reply string. It never does slow work itself: anything that takes time
 * (deploying, calibrating, camera actions) is recorded as a pending action
 * that the main loop or another task picks up.
 */

#define CMD_ECHO_SIZE 32
#define CMD_REPLY_SIZE 64

typedef enum {
    CMD_OK = 0,
    CMD_ERR_UNKNOWN,       /* no such command */
    CMD_ERR_RAN_ONCE,      /* run-once command already executed */
    CMD_ERR_NOT_SIM,       /* SIMP received while in FLIGHT mode */
    CMD_ERR_NOT_ENABLED,   /* "sim activate" before "sim enable" */
    CMD_ERR_BAD_ARG,       /* missing or invalid argument */
} cmd_result_t;

typedef enum {
    OP_MODE_FLIGHT = 0,
    OP_MODE_SIM = 1,
} op_mode_t;

/* Bits in once_mask: commands that may only run once. */
enum {
    ONCE_SOLAR_1 = 0,
    ONCE_SOLAR_2 = 1,
    ONCE_BOOM = 2,
    ONCE_CAM_START = 3,
};

/* Bits in pending_actions: work requested by a command, not yet done. */
enum {
    ACTION_DEPLOY_SOLAR_1   = 1u << 0,
    ACTION_DEPLOY_SOLAR_2   = 1u << 1,
    ACTION_DEPLOY_BOOM      = 1u << 2,
    ACTION_CAM_START        = 1u << 3,
    ACTION_CAM_ROTATE       = 1u << 4,
    ACTION_CAM_RECORD_START = 1u << 5,
    ACTION_CAM_RECORD_STOP  = 1u << 6,
    ACTION_CAM_STAB_CHANGED = 1u << 7,
    ACTION_CALIBRATE        = 1u << 8,
};

typedef struct {
    /* Reported in telemetry */
    uint16_t count;                 /* <cmd_count> */
    char echo[CMD_ECHO_SIZE];       /* <cmd_echo>, starts as "None" */
    uint8_t op_mode;                /* <op_mode>: 0 flight, 1 sim */

    /* Simulation mode */
    bool sim_enabled;               /* "sim enable" received */
    bool sim_pressure_valid;        /* at least one SIMP received */
    uint32_t sim_pressure_pa;       /* latest SIMP value */

    /* Run-once bookkeeping (should be saved to NVS later, requirement F8) */
    uint8_t once_mask;

    /* Camera settings */
    bool cam_stabilization;         /* on by default */
    int16_t cam_rotate_deg;         /* argument of the last "cam rotate" */

    /* Work for other code to do; clear each bit once it is handled */
    uint16_t pending_actions;
} command_state_t;

void command_state_init(command_state_t *s);

/*
 * Handle one command.
 *   cmd   : NUL-terminated command text
 *   s     : command state, updated in place
 *   reply : buffer for the reply text (use CMD_REPLY_SIZE)
 * Returns CMD_OK or an error code that can go in <error_code>.
 */
cmd_result_t command_handle(const char *cmd, command_state_t *s,
                            char *reply, size_t reply_size);