#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/eventfd.h>
#include <pthread.h>

#define cx_str_name cxstr
#define cx_str_static
#define cx_str_implement
#include "cx_str.h"

#define cx_array_name cxarr_str
#define cx_array_type cxstr
#define cx_array_static
#define cx_array_implement
#include "cx_array.h"

#include "linenoise.h"
#include "cli.h"

typedef struct Cli {
    const CliCmd*   cmds;       // Array of command descriptions (last command name = NULL);
    cxarr_str       args;       // Array of parsed arguments from command line
    bool            exit;
    struct linenoiseState ls;
    bool            linenoiseEdit;
    int             evfd;
    pthread_mutex_t lock;
} Cli;

Cli* cli_create(const CliCmd* cmds) {

    Cli* cli = calloc(1, sizeof(Cli));
    cli->cmds = cmds;
    cli->args = cxarr_str_init();

    cli->evfd = eventfd(0, 0);
    assert(cli->evfd >= 0);
    assert(pthread_mutex_init(&cli->lock, NULL) == 0);
    return cli;
}

void cli_destroy(Cli* cli) {

    close(cli->evfd);
    assert(pthread_mutex_destroy(&cli->lock) == 0);
    for (size_t i = 0; i < cxarr_str_len(&cli->args); i++) {
        cxstr_free(&cli->args.data[i]);
    }
    cxarr_str_free(&cli->args);
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
        FD_SET(cli->evfd, &readfds);
        int retval = select(cli->evfd+1, &readfds, NULL, NULL, NULL);
        if (retval == -1) {
            perror("select()");
            exit(1);
        } else if (retval >= 0) {
            if (cli->exit) {
                line = NULL;
                break;
            }
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

    for (size_t i = 0; i < cxarr_str_len(&cli->args); i++) {
        cxstr_free(&cli->args.data[i]);
    }
    cxarr_str_clear(&cli->args);

    char* p = line;
    while (*p) {
        // Ignore leading spaces
        if (isspace(*p)) {
            p++;
            continue;
        }
        // Checks for line termination
        if (!*p) {
            break;
        }
        // Start of argument
        char* arg_start = p;
        size_t arg_len = 0;
        // Looks for argument end or line termination
        while (*p && !isspace(*p)) {
            p++;
            arg_len++;
        }
        // Adds argument to array
        cxstr arg = cxstr_initn(arg_start, arg_len);
        cxarr_str_push(&cli->args, arg);
    }

    if (cxarr_str_len(&cli->args) == 0) {
        return CliEmptyLine;
    }

    // Find command with name equal to first argument
    const CliCmd* pc = cli->cmds;
    while (pc->name) {
        if (strcmp(pc->name, cli->args.data[0].data) == 0) {
            return pc->handler(cli, udata);
        }
        pc++;
    }
    return CliInvalidCmd;
}

int cli_argc(Cli* cli) {

    return cxarr_str_len(&cli->args);
}

const char* cli_argv(Cli* cli, size_t idx) {

    if (idx >= cxarr_str_len(&cli->args)) {
        return NULL;
    }
    return cli->args.data[idx].data;
}

void cli_force_exit(Cli* cli) {

    cli->exit = true;
    write(cli->evfd, (uint64_t[]){1}, sizeof(uint64_t));
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


