#include "command_parser.h"

#include <stdio.h>
#include <string.h>

void command_parse(const char *command, char *response, size_t response_size)
{
    if (command == NULL || response == NULL || response_size == 0) {
        return;
    }

    const char *result;
    if (strcmp(command, "PING") == 0) {
        result = "PONG";
    } else if (strcmp(command, "STATUS?") == 0) {
        result = "OK:READY";
    } else {
        result = "ERR:UNKNOWN_COMMAND";
    }
    snprintf(response, response_size, "%s", result);
}
