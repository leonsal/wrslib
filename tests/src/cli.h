#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// Result codes returned by cli_parse()
typedef enum {
    CliOk,
    CliEmptyLine,
    CliInvalidCmd,
    CliCmdError,
} CliResult;

// Type for command handlers
typedef struct Cli Cli;
typedef int (*CliCmdHandler)(Cli* cli, void* udata);

// Type for command description
typedef struct CliCmd {
    const char*     name;
    const char*     help;
    CliCmdHandler   handler;
} CliCmd;

// Creates and returns Cli instance for the specified commands array.
// The array must be terminated with a command with a NULL name.
Cli* cli_create(const CliCmd* cmds);

// Destroy previously create Cli instance
void cli_destroy(Cli* cli);

// Blocks reading line from the console (uses linenoise)
// Return NULL on Ctr-D
// The line must be freed after use.
char* cli_get_line(Cli* cli, const char* prompt);

void cli_lock_edit(Cli* cli);

void cli_unlock_edit(Cli* cli);

// Prints before line input
void cli_printf(Cli* cli, const char* fmt, ...);

// Parses the specified command line
CliResult cli_parse(Cli* cli, char* line, void* udata);

// Returns the current number of argument from the last call of cli_parse()
int cli_argc(Cli* cli);

// Returns pointer to specified argument from the last call of cli_parse()
const char* cli_argv(Cli* cli, size_t idx);

// Returns if exit command executed
bool cli_exit(Cli* cli);

// Default help command
int cli_cmd_help(Cli* cli, void* udata);

// Default exit command
int cli_cmd_exit(Cli* cli, void* udata);


#endif



