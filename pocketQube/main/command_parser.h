#pragma once

#include <stddef.h>

/* Convert a command payload to its response. Output is always NUL terminated. */
void command_parse(const char *command, char *response, size_t response_size);
