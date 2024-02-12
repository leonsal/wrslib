#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <pthread.h>

#include "linenoise.h"
#include "cli.h"

typedef struct Cli {
    const CliCmd*   cmds;       // Array of command descriptions (last command name = NULL);
    size_t          linecap;    // Current line capacity in bytes
    char*           line;       // Pointer to current line
    size_t          argcap;     // Current arguments array capacity
    size_t          argc;       // Current number of arguments
    char**          argv;       // Pointer to array of argument pointers
    bool            exit;
    struct linenoiseState ls;
    bool            linenoiseEdit;
    pthread_mutex_t lock;
} Cli;

Cli* cli_create(const CliCmd* cmds) {

    Cli* cli = calloc(1, sizeof(Cli));
    cli->cmds = cmds;

    cli->linecap = 132;
    cli->line = malloc(cli->linecap + 1);

    cli->argcap = 10;
    cli->argv = malloc((cli->argcap+1) * sizeof(char*));
    assert(pthread_mutex_init(&cli->lock, NULL) == 0);
    return cli;
}

void cli_destroy(Cli* cli) {

    assert(pthread_mutex_destroy(&cli->lock) == 0);
    free(cli->argv);
    free(cli->line);
    free(cli);
}

char* cli_get_line(Cli* cli, const char* prompt) {

    char* line;
    char buf[1024];
    cli->linenoiseEdit = true;
    linenoiseEditStart(&cli->ls,-1,-1,buf,sizeof(buf), prompt);
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(cli->ls.ifd, &readfds);
        int retval = select(cli->ls.ifd+1, &readfds, NULL, NULL, NULL);
        if (retval == -1) {
            perror("select()");
            exit(1);
        } else if (retval >= 0) {
            assert(pthread_mutex_lock(&cli->lock) == 0);
            line = linenoiseEditFeed(&cli->ls);
            assert(pthread_mutex_unlock(&cli->lock) == 0);
            if (line != linenoiseEditMore) {
                break;
            }
        }
    } 
    linenoiseEditStop(&cli->ls);
    cli->linenoiseEdit = false;
    return line;
}

void cli_lock_edit(Cli* cli) {

    if (cli->linenoiseEdit) {
        assert(pthread_mutex_lock(&cli->lock) == 0);
        linenoiseHide(&cli->ls);
    }
}

void cli_unlock_edit(Cli* cli) {

    if (cli->linenoiseEdit) {
        linenoiseShow(&cli->ls);
        assert(pthread_mutex_unlock(&cli->lock) == 0);
    }
}

void cli_printf(Cli* cli, const char* fmt, ...) {

    va_list ap;
    va_start(ap, fmt);

    cli_lock_edit(cli);
    vprintf(fmt, ap);
    va_end(ap);
    cli_unlock_edit(cli);
}


CliResult cli_parse(Cli* cli, char* line, void* udata) {

    const size_t len = strlen(line);
    if (len > cli->linecap) {
        cli->line = realloc(cli->line, len + 1);
        cli->linecap = len;
    }
    strcpy(cli->line, line);

    cli->argc = 0;
    char* p = cli->line;
    while (*p) {
        // Ignore leading spaces
        if (isspace(*p)) {
            p++;
            continue;
        }
        if (!*p) {
            break;
        }
        // Start of argument
        cli->argv[cli->argc] = p;
        cli->argc++;
        // Looks for argument end
        while (*p && !isspace(*p)) {
            p++;
        }
        if (!*p) {
            break;
        }
        // Terminates the argument string
        *p = 0;
        p++;
        // Reallocates array of arguments pointer if necessary
        if (cli->argc >= cli->argcap) {
            cli->argcap *= 2;
            cli->argv = realloc(cli->argv, (cli->argcap+1) * sizeof(char*));
        }
    }

    if (cli->argc == 0) {
        return CliEmptyLine;
    }

    // Find command with name equal to first argument
    const CliCmd* pc = cli->cmds;
    while (pc->name) {
        if (strcmp(pc->name, cli->argv[0]) == 0) {
            return pc->handler(cli, udata);
        }
        pc++;
    }
    return CliInvalidCmd;
}

int cli_argc(Cli* cli) {

    return cli->argc;
}

const char* cli_argv(Cli* cli, size_t idx) {

    if (idx >= cli->argc) {
        return NULL;
    }
    return cli->argv[idx];
}

bool cli_exit(Cli* cli) {

    return cli->exit;
}

int cli_cmd_help(Cli* cli, void* udata) {

    const CliCmd* pc = cli->cmds;
    while (pc->name) {
        printf("  %s - %s\n", pc->name, pc->help);
        pc++;
    }
    return CliOk;
}

int cli_cmd_exit(Cli* cli, void* udata) {

    cli->exit = true;
    return CliOk;
}


