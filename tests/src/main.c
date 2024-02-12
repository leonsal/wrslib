#include <stdio.h>
#include <unistd.h>

#include "cx_alloc.h"

#include "argparse.h"
#include "cli.h"
#include "wrs.h"

// Static filesystem symbols
extern const unsigned char gStaticfsZipData[];
extern const unsigned int  gStaticfsZipSize;

// Spplication state
typedef struct AppState {
    Cli*            cli;
    Wrs*            wrs;
    WrsRpc*         rpc1;
    WrsRpc*         rpc2;
    int             server_port;        // HTTP server listening port
    bool            use_staticfs;       // Use external app file system for development
    _Atomic bool    run_server;
    // queue           queue_chartjs;
    // pthread_t       thread_chartjs_id;
    // ThreadArg       thread_arg;
    // _Atomic bool    run_server;
} AppState;

// Forward declarations
void log_console_handler(wrs_logger* l, CxLogEvent *ev);
static CliCmd cmds[];
static int parse_options(int argc, const char* argv[], AppState* apps);
static void command_line_loop(AppState* app);
static void rpc_event(WrsRpc* rpc, size_t connid, WrsEvent ev);
static int rpc_get_time(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
static int rpc_get_lines(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
static int cmd_call(Cli* cli, void* udata);

#define CHKT(COND) \
    { if (!COND) {fprintf(stderr, "ERROR %s/line:%d\n", __func__, __LINE__); abort();}}
#define CHKF(COND) \
    { if (COND) {fprintf(stderr, "ERROR %s/line:%d\n", __func__, __LINE__); abort();}}

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
    wrs_logger_add_handler(&wrs_default_logger, log_console_handler, &app, CX_LOG_DEBUG);
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

    // Creates server, rpc endpoints and bindings
    app.wrs = wrs_create(&cfg);
    app.rpc1 = wrs_rpc_open(app.wrs, "/rpc1", 2, rpc_event);
    CHKF(wrs_rpc_bind(app.rpc1, "get_time", rpc_get_time));
    CHKF(wrs_rpc_bind(app.rpc1, "get_lines", rpc_get_lines));

    // Blocks processing commands
    command_line_loop(&app);

    cli_destroy(app.cli);
    wrs_destroy(app.wrs);
    return 0;
}

// Calls the original cx_log console handler 
void log_console_handler(wrs_logger* l, CxLogEvent *ev) {

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
    {
        .name = "call",
        .help = "Call browser func",
        .handler = cmd_call,
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

static void command_line_loop(AppState* app) {

    // Command line loop
    while(!cli_exit(app->cli)) {

        // Read command line
        char* line = cli_get_line(app->cli, ">");
        if (line == NULL) {
            break;
        }
        // Parse comand line and execute command if possible
        int res = cli_parse(app->cli, line, app);
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
        linenoiseHistoryAdd(line);
    }
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

static int rpc_get_time(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp) {

    cx_var_set_map_int(resp, "data", 42);
    return 0;
}

static int rpc_get_lines(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp) {

    // Get request parameters
    int64_t count;
    int64_t len;
    CHKT(cx_var_get_map_int(params, "count", &count));
    CHKT(cx_var_get_map_int(params, "len", &len));

    // Creates response 'data' as an array:
    CxVar* arr = cx_var_set_map_arr(resp, "data");

    // Creates and pushes string lines into the array
    char* line = malloc(len);
    for (size_t l = 0; l < count; l++) {
        int code = '!';
        for (size_t c = 0; c < len; c++) {
            line[c] = code++;
            if (code >= '~') {
                code = '!';
            }
        }
       CHKT(cx_var_push_arr_strn(arr, line, len));
    }
    free(line);
    return 0;
}

static int cmd_call(Cli* cli, void* udata) {

   AppState* app = udata;
   const size_t connid = 0;
   CxVar* params = cx_var_new(cxDefaultAllocator());
   wrs_rpc_call(app->rpc1, connid, "sum_array", params, NULL);
   return CliOk;
}


