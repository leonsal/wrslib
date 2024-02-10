#include <stdio.h>
#include <unistd.h>

#include "argparse.h"
#include "wrc.h"

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


int main(int argc, const char* argv[]) {

    // Initializes WRC logger
    wrc_logger_set_flags(&wrc_default_logger, CX_LOG_FLAG_TIME|CX_LOG_FLAG_US|CX_LOG_FLAG_COLOR);
    wrc_logger_add_handler(&wrc_default_logger, wrc_logger_console_handler, NULL, CX_LOG_DEBUG);
    WRC_LOGD("WRT tests");

    // Parse command line options
    AppState apps = {
        .server_port = 8888,
        .run_server = true,
        
    };
    parse_options(argc, argv, &apps);

    // Sets server config
    WrcConfig cfg = {
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

    // Createsserver
    Wrc* wrc = wrc_create(&cfg);

    while (apps.run_server) {
        sleep(1);
    }

    wrc_destroy(wrc);

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
