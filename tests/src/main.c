#include <stdio.h>

#include "argparse.h"
#include "wrc.h"

// Test application state
typedef struct AppState {
    int     server_port;        // HTTP server listening port
    bool    use_staticfs;       // Use external app file system for development
    // queue           queue_chartjs;
    // pthread_t       thread_chartjs_id;
    // ThreadArg       thread_arg;
    // _Atomic bool    run_server;
} AppState;

static int parse_options(int argc, const char* argv[], AppState* apps);

int main(int argc, const char* argv[]) {

    // Initializes WRC logger
    wrc_logger_set_flags(&wrc_default_logger, CX_LOG_FLAG_TIME|CX_LOG_FLAG_US|CX_LOG_FLAG_COLOR);
    wrc_logger_add_handler(&wrc_default_logger, wrc_logger_console_handler, NULL, CX_LOG_DEBUG);
    WRC_LOGD("WRT tests");

    // Parse command line options
    AppState apps = {
        .server_port = 8888
    };
    parse_options(argc, argv, &apps);
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
