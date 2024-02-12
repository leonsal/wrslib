#include <stdio.h>
#include <unistd.h>

#include "argparse.h"
#include "cli.h"
#include "wrs.h"

// Static filesystem symbols
extern const unsigned char gStaticfsZipData[];
extern const unsigned int  gStaticfsZipSize;

// Test application state
typedef struct AppState {
    Cli*    cli;
    Wrs*    wrs;
    int    server_port;        // HTTP server listening port
    bool   use_staticfs;       // Use external app file system for development
    _Atomic bool    run_server;
    // queue           queue_chartjs;
    // pthread_t       thread_chartjs_id;
    // ThreadArg       thread_arg;
    // _Atomic bool    run_server;
} AppState;

// Forward declarations
void log_print(wrs_logger* l, CxLogEvent *ev);
static CliCmd cmds[];
static int parse_options(int argc, const char* argv[], AppState* apps);
static void rpc_event(WrsRpc* rpc, size_t connid, WrsEvent ev);


int main(int argc, const char* argv[]) {

    // Initialize app state
    AppState app = {
        .server_port = 8888,
        .run_server = true,
    };
    app.cli = cli_create(cmds);
    parse_options(argc, argv, &app);

    // Initializes WRC logger using special console handler
    // which prints safely above command line being edited
    wrs_logger_set_flags(&wrs_default_logger, CX_LOG_FLAG_TIME|CX_LOG_FLAG_US|CX_LOG_FLAG_COLOR);
    wrs_logger_add_handler(&wrs_default_logger, log_print, &app, CX_LOG_DEBUG);
    WRS_LOGD("WRT tests");

    // Sets server config
    WrsConfig cfg = {
        .document_root       = "./src/staticfs",
        .listening_port      = app.server_port,
        .use_staticfs        = app.use_staticfs,
        .staticfs_prefix     = "staticfs",
        .staticfs_data       = gStaticfsZipData,
        .staticfs_len        = gStaticfsZipSize,
        .browser = {
            .start = false,
            .standard = false,
            .cmd_line = {"google-chrome --app="},
        },
    };

    // Creates server
    app.wrs = wrs_create(&cfg);

    // Command line loop
    while(!cli_exit(app.cli)) {

        // Read command line
        char* line = cli_get_line(app.cli, ">");
        if (line == NULL) {
            break;
        }
        // Parse comand line and execute command if possible
        int res = cli_parse(app.cli, line, &app);
        if (res == CliEmptyLine) {
            continue;
        }
        if (res == CliInvalidCmd) {
            printf("Invalid command\n");
        }
        if (res < 0) {
            printf("%s\n", strerror(-res));
        }
        // Adds command line to history
        //linenoiseHistoryAdd(line);
    }

    wrs_destroy(app.wrs);

    //
    // // Creates RPC endpoints
    // WrsRpc* rpc1  = wrs_rpc_open(wrs, "/rpc1", 2, rpc_event);
    // assert(rpc1);
    //
    // // // Set bindings
    // // res = wui_bind_rpc(wa, RPC_URL, "get_time", rpc_get_time);
    // // assert(res == 0);
    // //
    // // res = wui_bind_rpc(wa, RPC_URL, "get_lines", rpc_get_lines);
    //
    // // Waits till server is stopped by test UI
    // while (apps.run_server) {
    //     sleep(1);
    // }
    //
    // wrs_destroy(wrs);
    //
    return 0;
}

void log_print(wrs_logger* l, CxLogEvent *ev) {

    AppState* app = ev->hdata;
    cli_lock_edit(app->cli);
    wrs_logger_console_handler(l, ev);
    cli_unlock_edit(app->cli);
}

static CliCmd cmds[] = {
    {
        .name = "help",
        .help = "List available commands",
        .handler = cli_cmd_help,
    },
    {
        .name = "exit",
        .help = "Exit program",
        .handler = cli_cmd_exit,
    },
};

static int parse_options(int argc, const char* argv[], AppState* apps) {

    static const char* usages[] = {
        "tests [options]",
        NULL,
    };
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_INTEGER('p', "port", &apps->server_port, "HTTP Server listening port", NULL, 0, 0),
        OPT_BOOLEAN('s', "staticfs", &apps->use_staticfs, "Use internal static filesystem", NULL, 0, 0),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "WRC Tests", NULL);
    argc = argparse_parse(&argparse, argc, argv);
    return 0;
}

static void rpc_event(WrsRpc* rpc, size_t connid, WrsEvent ev) {

    WrsRpcInfo info = info = wrs_rpc_info(rpc);
    char* evname = "?";
    switch (ev) {
        case WrsEventOpen:
            evname = "Open";
            break;
        case WrsEventClose:
            evname = "Close";
            break;
        case WrsEventReady:
            evname = "Ready";
            break;
        default:
            break;
    }
    WRS_LOGD("%s: handler:%s connid:%zu event:%s", __func__, info.url, connid, evname);
}

