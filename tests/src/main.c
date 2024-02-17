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
    size_t          test_bin_count;
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
static int rpc_server_text_msg(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
static int rpc_server_bin_msg(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
static int cmd_test_bin(Cli* cli, void* udata);
static void call_test_bin(WrsRpc* rpc, size_t size);
static int resp_test_bin(WrsRpc* rpc, size_t connid, CxVar* resp);

#define CHKT(COND) \
    { if (!(COND)) {fprintf(stderr, "CHK ERROR in %s() at %s:%d\n", __func__, __FILE__, __LINE__); abort();}}
#define CHKF(COND) \
    { if (COND) {fprintf(stderr, "CHK ERROR in %s() at %s:%d\n", __func__, __FILE__, __LINE__); abort();}}

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
    wrs_rpc_set_userdata(app.rpc1, &app);
    CHKF(wrs_rpc_bind(app.rpc1, "rpc_server_text_msg", rpc_server_text_msg));
    CHKF(wrs_rpc_bind(app.rpc1, "rpc_server_bin_msg", rpc_server_bin_msg));

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
        .name = "test_bin",
        .help = "Test calling browser with binary arrays",
        .handler = cmd_test_bin,
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
            continue;
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

static int rpc_server_text_msg(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp) {

    // Get message parameters
    int64_t size;
    CHKT(cx_var_get_map_int(params, "size", &size));
    CxVar* arr_in = cx_var_get_map_arr(params, "arr");
    size_t arr_in_len;
    CHKT(cx_var_get_arr_len(arr_in, &arr_in_len));
    CHKT(arr_in_len == size);

    // Creates response 'data'
    CxVar* map = cx_var_set_map_map(resp, "data");
    cx_var_set_map_int(map, "size", size);
    // Array with input elements incremented
    CxVar* arr_out = cx_var_set_map_arr(map, "arr");
    for (size_t i = 0; i < size; i++) {
        int64_t el;
        cx_var_get_arr_int(arr_in, i, &el);
        cx_var_push_arr_int(arr_out, el+1);
    }
    return 0;
}

static int rpc_server_bin_msg(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp) {

    //
    // Get message parameters
    //
    int64_t size;
    CHKT(cx_var_get_map_int(params, "size", &size));

    uint8_t* u8;
    size_t u8len;
    CHKT(cx_var_get_map_buf(params, "u8", (void*)&u8, &u8len));
    WRS_LOGD("%s: u8_len:%zu", __func__, u8len);

    uint16_t* u16;
    size_t u16len;
    CHKT(cx_var_get_map_buf(params, "u16", (void*)&u16, &u16len));
    WRS_LOGD("%s: u16_len:%zu", __func__, u16len);

    uint32_t* u32;
    size_t u32len;
    CHKT(cx_var_get_map_buf(params, "u32", (void*)&u32, &u32len));
    WRS_LOGD("%s: u32_len:%zu", __func__, u32len);

    float* f32;
    size_t f32len;
    CHKT(cx_var_get_map_buf(params, "f32", (void*)&f32, &f32len));
    WRS_LOGD("%s: f32_len:%zu", __func__, f32len);

    double* f64;
    size_t f64len;
    CHKT(cx_var_get_map_buf(params, "f64", (void*)&f64, &f64len));
    WRS_LOGD("%s: f64_len:%zu", __func__, f64len);

    //
    // Creates response 'data'
    //
    CxVar* map = cx_var_set_map_map(resp, "data");
    cx_var_set_map_int(map, "size", size);

    // Create and fill buffers
    uint8_t* ru8;
    size_t ru8len;
    cx_var_get_buf(cx_var_set_map_buf(map, "u8", NULL, size * sizeof(uint8_t)), (void*)&ru8, &ru8len);
    for (size_t i = 0; i < size; i++) {
        ru8[i] = u8[i]+1;
    }

    uint16_t* ru16;
    size_t ru16len;
    cx_var_get_buf(cx_var_set_map_buf(map, "u16", NULL, size * sizeof(uint16_t)), (void*)&ru16, &ru16len);
    for (size_t i = 0; i < size; i++) {
        ru16[i] = u16[i]+1;
    }

    uint32_t* ru32;
    size_t ru32len;
    cx_var_get_buf(cx_var_set_map_buf(map, "u32", NULL, size * sizeof(uint32_t)), (void*)&ru32, &ru32len);
    for (size_t i = 0; i < size; i++) {
        ru32[i] = u32[i]+1;
    }

    float* rf32;
    size_t rf32len;
    cx_var_get_buf(cx_var_set_map_buf(map, "f32", NULL, size * sizeof(float)), (void*)&rf32, &rf32len);
    for (size_t i = 0; i < size; i++) {
        rf32[i] = f32[i]+1;
    }

    double* rf64;
    size_t rf64len;
    cx_var_get_buf(cx_var_set_map_buf(map, "f64", NULL, size * sizeof(double)), (void*)&rf64, &rf64len);
    for (size_t i = 0; i < size; i++) {
        rf64[i] = f64[i]+1;
    }

    return 0;
}


static int cmd_test_bin(Cli* cli, void* udata) {

   AppState* app = udata;
   ssize_t size  = 10;
   if (cli_argc(cli) > 1) {
       app->test_bin_count = strtol(cli_argv(cli, 1), NULL, 10);
   }
   if (cli_argc(cli) > 2) {
       size = strtol(cli_argv(cli, 2), NULL, 10);
   }

   call_test_bin(app->rpc1, size);
   return CliOk;
}

static void call_test_bin(WrsRpc* rpc, size_t size) {

    // Create parameters with non-initialized buffers
    CxVar* params = cx_var_new(cxDefaultAllocator());
    cx_var_set_map(params);
    cx_var_set_map_buf(params, "u32", NULL, size * sizeof(uint32_t));
    cx_var_set_map_buf(params, "f32", NULL, size * sizeof(float));
    cx_var_set_map_buf(params, "f64", NULL, size * sizeof(double));

    // Get buffers and initialize them
    size_t len;
    uint32_t* arru32;
    cx_var_get_map_buf(params, "u32", (void*)&arru32, &len);
    for (size_t i = 0; i < size; i++) {
         arru32[i] = i;
    }
    float* arrf32;
    cx_var_get_map_buf(params, "f32", (void*)&arrf32, &len);
    for (size_t i = 0; i < size; i++) {
         arrf32[i] = i*2;
    }
    double* arrf64;
    cx_var_get_map_buf(params, "f64", (void*)&arrf64, &len);
    for (size_t i = 0; i < size; i++) {
         arrf64[i] = i*3;
    }

    int res = wrs_rpc_call(rpc, 0, "test_bin", params, resp_test_bin);
    if (res) {
        WRS_LOGE("%s: error from wrs_rpc_call()", __func__);
        return;
    }
    WRS_LOGD("%s: called test_bin", __func__);
    cx_var_del(params);
}

static int resp_test_bin(WrsRpc* rpc, size_t connid, CxVar* resp) {

    AppState* app = wrs_rpc_get_userdata(rpc);
    WRS_LOGD("%s: response test_bin", __func__);
    CxVar* data = cx_var_get_map_val(resp, "data");
    CHKT(data);

    const uint32_t* u32;
    size_t u32_len;
    CHKT(cx_var_get_map_buf(data, "u32", (const void**)&u32, &u32_len));
    for (size_t i = 0; i < u32_len/sizeof(uint32_t); i++) {
         if (u32[i] != (i+1)) {
            WRS_LOGE("%s: u32 response error", __func__);
            break;
         }
    }

    const float* f32;
    size_t f32_len;
    CHKT(cx_var_get_map_buf(data, "f32", (const void**)&f32, &f32_len));
    for (size_t i = 0; i < f32_len/sizeof(float); i++) {
         if (f32[i] != (i*2+1)) {
            WRS_LOGE("%s: f32 response error", __func__);
            break;
         }
    }

    const double* f64;
    size_t f64_len;
    CHKT(cx_var_get_map_buf(data, "f64", (const void**)&f64, &f64_len));
    for (size_t i = 0; i < f64_len/sizeof(double); i++) {
         if (f64[i] != (i*3+1)) {
            WRS_LOGE("%s: f64 response error", __func__);
            break;
         }
    }

    if (app->test_bin_count > 0) {
        call_test_bin(rpc, u32_len/sizeof(uint32_t));
        app->test_bin_count--; 
    }
    
    return 0;
}


