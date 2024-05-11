#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include "cx_alloc.h"
#include "cx_logger.h"

#include "argparse.h"
#include "cli.h"
//#include "webkit.h"
#include "wrs.h"

// Static filesystem symbols
extern const unsigned char gStaticfsZipData[];
extern const unsigned int  gStaticfsZipSize;

// Chart state
typedef struct Audio {
    int64_t sample_rate;    // sample rate in Hz
    int64_t gain;           // gain (0-100%)
    int64_t freq;           // frequency Hz
    int64_t noise;          // noise ampliture (0-100%)
    int64_t nsamples;       // number of samples to generate
    double  phase;
} Audio;

// Application state
typedef struct AppState {
    Cli*            cli;
    Wrs*            wrs;
    WrsRpc*         rpc1;
    WrsRpc*         rpc2;
    int             server_port;        // HTTP server listening port
    bool            use_staticfs;       // Use external app file system for development
    bool            webkit;             // Uses internal webkit gtk view
    bool            start_browser;   
    _Atomic bool    run_server;
    size_t          test_bin_count;
    Audio           audio;
} AppState;

// Forward declarations
void log_console_handler(const CxLogger* logger, CxLoggerEvent *ev);
static CliCmd cmds[];
static int parse_options(int argc, const char* argv[], AppState* apps);
static void command_line_loop(AppState* app);
static void rpc_event(WrsRpc* rpc, size_t connid, WrsEvent ev);
static int rpc_server_text_msg(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
static int rpc_server_bin_msg(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
static int rpc_server_audio_set(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
static int rpc_server_audio_run(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
static int rpc_server_exit(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);
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
    wrs_default_logger = cx_logger_new(NULL, NULL);
    cx_logger_set_flags(wrs_default_logger, CxLoggerFlagTime|CxLoggerFlagUs|CxLoggerFlagColor);
    cx_logger_add_handler(wrs_default_logger, log_console_handler, &app);
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

    if (app.start_browser) {
        cfg.browser.start = true;
    }

    // Creates server
    app.wrs = wrs_create(&cfg);

    // Creates RPC 1
    app.rpc1 = wrs_rpc_open(app.wrs, "/rpc1", 2, rpc_event);
    wrs_rpc_set_userdata(app.rpc1, &app);
    CXERROR_CHK(wrs_rpc_bind(app.rpc1, "rpc_server_text_msg", rpc_server_text_msg));
    CXERROR_CHK(wrs_rpc_bind(app.rpc1, "rpc_server_bin_msg", rpc_server_bin_msg));
    CXERROR_CHK(wrs_rpc_bind(app.rpc1, "rpc_server_exit", rpc_server_exit));

    // Creates RPC 2
    app.rpc2 = wrs_rpc_open(app.wrs, "/rpc2", 2, rpc_event);
    wrs_rpc_set_userdata(app.rpc2, &app);
    CXERROR_CHK(wrs_rpc_bind(app.rpc2, "rpc_server_audio_set", rpc_server_audio_set));
    CXERROR_CHK(wrs_rpc_bind(app.rpc2, "rpc_server_audio_run", rpc_server_audio_run));

    // if (app.webkit) {
    //     webkit_start(argc, (char**)argv, "http://localhost:8888");
    // } else {
        // Blocks processing commands
        command_line_loop(&app);
    //}

    WRS_LOGI("Terminating...");
    cx_logger_del(wrs_default_logger);
    cli_destroy(app.cli);
    wrs_destroy(app.wrs);
    return 0;
}

// Calls the original cx_log console handler 
void log_console_handler(const CxLogger* logger, CxLoggerEvent *ev) {

    AppState* app = ev->hdata;
    cli_lock_edit(app->cli);
    cx_logger_console_handler(logger, ev);
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
        .name = "call_test_bin",
        .help = "Call browser with binary arrays: [<count> [<size>]]",
        .handler = cmd_test_bin,
    },
    {0}
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
        OPT_BOOLEAN('w', "webview", &apps->webkit, "Uses internal Webkit GTK view", NULL, 0, 0),
        OPT_BOOLEAN('b', "browser", &apps->start_browser, "Starts default browser", NULL, 0, 0),
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
        if (res == CliOk) {
            // Adds command line to history
            linenoiseHistoryAdd(line);
            free(line);
            continue;
        }
        free(line);
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

    CxVar* u8 = cx_var_get_map_arr(params, "u8");
    size_t u8len;
    CHKT(cx_var_get_arr_len(u8, &u8len));
    CHKT(u8len == size);

    CxVar* u16 = cx_var_get_map_arr(params, "u16");
    size_t u16len;
    CHKT(cx_var_get_arr_len(u16, &u16len));
    CHKT(u16len == size);

    CxVar* u32 = cx_var_get_map_arr(params, "u32");
    size_t u32len;
    CHKT(cx_var_get_arr_len(u32, &u32len));
    CHKT(u32len == size);

    CxVar* f32 = cx_var_get_map_arr(params, "f32");
    size_t f32len;
    CHKT(cx_var_get_arr_len(f32, &f32len));
    CHKT(f32len == size);

    CxVar* f64 = cx_var_get_map_arr(params, "f64");
    size_t f64len;
    CHKT(cx_var_get_arr_len(f64, &f64len));
    CHKT(f64len == size);

    //
    // Creates response 'data'
    //
    CxVar* map = cx_var_set_map_map(resp, "data");
    cx_var_set_map_int(map, "size", size);

    CxVar* u8out = cx_var_set_map_arr(map, "u8");
    for (size_t i = 0; i < size; i++) {
        int64_t el;
        cx_var_get_arr_int(u8, i, &el);
        cx_var_push_arr_int(u8out, el+1);
    }

    CxVar* u16out = cx_var_set_map_arr(map, "u16");
    for (size_t i = 0; i < size; i++) {
        int64_t el;
        cx_var_get_arr_int(u16, i, &el);
        cx_var_push_arr_int(u16out, el+1);
    }

    CxVar* u32out = cx_var_set_map_arr(map, "u32");
    for (size_t i = 0; i < size; i++) {
        int64_t el;
        cx_var_get_arr_int(u32, i, &el);
        cx_var_push_arr_int(u32out, el+1);
    }

    CxVar* f32out = cx_var_set_map_arr(map, "f32");
    for (size_t i = 0; i < size; i++) {
        int64_t el;
        cx_var_get_arr_int(f32, i, &el);
        cx_var_push_arr_int(f32out, el+1);
    }

    CxVar* f64out = cx_var_set_map_arr(map, "f64");
    for (size_t i = 0; i < size; i++) {
        int64_t el;
        cx_var_get_arr_int(f64, i, &el);
        cx_var_push_arr_int(f64out, el+1);
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

    uint16_t* u16;
    size_t u16len;
    CHKT(cx_var_get_map_buf(params, "u16", (void*)&u16, &u16len));

    uint32_t* u32;
    size_t u32len;
    CHKT(cx_var_get_map_buf(params, "u32", (void*)&u32, &u32len));

    float* f32;
    size_t f32len;
    CHKT(cx_var_get_map_buf(params, "f32", (void*)&f32, &f32len));

    double* f64;
    size_t f64len;
    CHKT(cx_var_get_map_buf(params, "f64", (void*)&f64, &f64len));

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

static int rpc_server_audio_set(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp) {

    AppState* app = wrs_rpc_get_userdata(rpc);
    cx_var_get_map_int(params, "sample_rate", &app->audio.sample_rate);
    cx_var_get_map_int(params, "nsamples", &app->audio.nsamples);
    cx_var_get_map_int(params, "gain", &app->audio.gain);
    cx_var_get_map_int(params, "freq", &app->audio.freq);
    cx_var_get_map_int(params, "noise", &app->audio.noise);
    WRS_LOGD("%s: freq:%ld, nsamples:%ld", __func__, app->audio.freq, app->audio.nsamples);
    return 0;
}

static int rpc_server_audio_run(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp) {

    //WRS_LOGD("%s:", __func__);
    AppState* app = wrs_rpc_get_userdata(rpc);

    // Creates response buffers and get address to its data
    CxVar* map = cx_var_set_map_map(resp, "data");
    CxVar* signal = cx_var_set_map_buf(map, "signal", NULL, app->audio.nsamples * sizeof(float));
    CxVar* label  = cx_var_set_map_buf(map, "label", NULL, app->audio.nsamples * sizeof(float));
    float* signal_data;
    size_t len;
    cx_var_get_buf(signal, (void*)&signal_data, &len);
    float* label_data;
    cx_var_get_buf(label, (void*)&label_data, &len);

    // Generates signal and labels
    const double delta = 2*M_PI * (double)app->audio.freq / (double)app->audio.sample_rate;
    for (size_t i = 0; i < app->audio.nsamples; i++) {
        signal_data[i] = ((double)app->audio.gain/100.0) * sin(app->audio.phase);
        const float noise = (50-(rand() % 100)) * app->audio.noise / 20000.0;
        signal_data[i] += noise;
        label_data[i] = i;
        app->audio.phase += delta;
        if (app->audio.phase >= 2*M_PI) {
            app->audio.phase = 2*M_PI-app->audio.phase;
        }
    }

    return 0;
}

static int rpc_server_exit(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp) {

    AppState* app = wrs_rpc_get_userdata(rpc);
    app->run_server = false;
    cli_force_exit(app->cli);
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
    // NOTE: params will be deallocated by wrs_rpc_call()
    CxVar* params = cx_var_new(cx_def_allocator());
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

    CxError err = wrs_rpc_call(rpc, 0, "test_bin", params, resp_test_bin);
    if (err.code) {
        WRS_LOGE("%s: error from wrs_rpc_call()", __func__);
        return;
    }
    WRS_LOGD("%s: called test_bin", __func__);
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


