#include <stdio.h>
#include <unistd.h>

#include "argparse.h"
#include "wrs.h"

// Static filesystem symbols
extern const unsigned char gStaticfsZipData[];
extern const unsigned int  gStaticfsZipSize;

// Test application state
typedef struct AppState {
    int    server_port;        // HTTP server listening port
    bool   use_staticfs;       // Use external app file system for development
    _Atomic bool    run_server;
    // queue           queue_chartjs;
    // pthread_t       thread_chartjs_id;
    // ThreadArg       thread_arg;
    // _Atomic bool    run_server;
} AppState;

// Forward declaration of local functions
static int parse_options(int argc, const char* argv[], AppState* apps);
static void rpc_event(WrsRpc* rpc, size_t connid, WrsEvent ev);


int main(int argc, const char* argv[]) {

    // Initializes WRC logger
    wrs_logger_set_flags(&wrs_default_logger, CX_LOG_FLAG_TIME|CX_LOG_FLAG_US|CX_LOG_FLAG_COLOR);
    wrs_logger_add_handler(&wrs_default_logger, wrs_logger_console_handler, NULL, CX_LOG_DEBUG);
    WRS_LOGD("WRT tests");

    // Parse command line options
    AppState apps = {
        .server_port = 8888,
        .run_server = true,
        
    };
    parse_options(argc, argv, &apps);

    // Sets server config
    WrsConfig cfg = {
        .document_root       = "./src/staticfs",
        .listening_port      = apps.server_port,
        .use_staticfs        = apps.use_staticfs,
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
    Wrs* wrs = wrs_create(&cfg);

    // Creates RPC endpoints
    WrsRpc* rpc1  = wrs_rpc_open(wrs, "/rpc1", 2, rpc_event);
    assert(rpc1);

    // // Set bindings
    // res = wui_bind_rpc(wa, RPC_URL, "get_time", rpc_get_time);
    // assert(res == 0);
    //
    // res = wui_bind_rpc(wa, RPC_URL, "get_lines", rpc_get_lines);

    // Waits till server is stopped by test UI
    while (apps.run_server) {
        sleep(1);
    }

    wrs_destroy(wrs);

    return 0;
}


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

