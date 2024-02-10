#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "civetweb.h"
#include "zip.h"

#include "cx_alloc.h"
#include "cx_pool_allocator.h"
#include "cx_timer.h"
#define WRC_LOG_IMPLEMENT
#include "wrc.h"

// Define internal array of server options
#define cx_array_name arr_opt
#define cx_array_type const char*
#define cx_array_implement
#define cx_array_static
#define cx_array_instance_allocator
#include "cx_array.h"

// Global logger (can be used by dependants)
wrc_logger wrc_default_logger;

// WRC server internal state
typedef struct Wrc {
    WrcConfig           cfg;            // Copy of user configuration
    CxPoolAllocator*    pool_alloc;     // Pool allocator
    const CxAllocator*  alloc;          // Allocator interface for Pool Allocator
    arr_opt             options;        // Array of server options
    int                 used_port;      // Used TCP/IP listening port
    CxTimer*            tm;             // Timer manager
    pthread_mutex_t     lock;           // For exclusive access to this state
    struct mg_context*  ctx;            // CivitWeb context
    zip_source_t*       zip_src;        // For zip static filesystem
    zip_t*              zip;            // For zip static filesystem
    //map_rpc             rpc_handlers;   // Map url to web socket rpc handler
    void*               userdata;       // Optional userdata
} Wrc;


// Forward declarations of local functions
static int wrc_find_port(Wrc* wrc);
static int wrc_zip_file_handler(struct mg_connection *conn, void *cbdata);
static int wrc_start_browser(Wrc* wrc);


Wrc* wrc_create(const WrcConfig* cfg) {

    // Creates allocator to use
    CxPoolAllocator* pool_alloc = cx_pool_allocator_create(4*1024, NULL);
    const CxAllocator* alloc = cx_pool_allocator_iface(pool_alloc);

    // Creates and initializes internal state
    Wrc* wrc = cx_alloc_malloc(alloc, sizeof(Wrc));
    if (wrc == NULL) {
        return NULL;
    }
    memset(wrc, 0, sizeof(Wrc));
    wrc->cfg = *cfg;
    wrc->pool_alloc = pool_alloc;
    wrc->alloc = alloc;
    //wrc->rpc_handlers = map_rpc_init(alloc, 17);
    assert(pthread_mutex_init(&wrc->lock, NULL) == 0);

    // If configured listening port is 0, finds an unused port
    wrc->used_port = cfg->listening_port;
    if (cfg->listening_port == 0) {
        int port = wrc_find_port(wrc);
        if (port < 0) {
            return NULL;
        }
        wrc->used_port = port;
    }

    // Builds server options array
    wrc->options = arr_opt_init(alloc);
    if (cfg->document_root) {
        arr_opt_push(&wrc->options, "document_root");
        char* document_root = cx_alloc_malloc(alloc, strlen(cfg->document_root)+1);
        strcpy(document_root, cfg->document_root);
        arr_opt_push(&wrc->options, document_root);
    }
    // Sets listening port
    const size_t size = 32;
    char* listening_ports = cx_alloc_malloc(alloc, size);
    snprintf(listening_ports, size-1, "%u", wrc->used_port);
    arr_opt_push(&wrc->options, "listening_ports");
    arr_opt_push(&wrc->options, listening_ports);
    // Options array terminator
    arr_opt_push(&wrc->options, NULL);
    arr_opt_push(&wrc->options, NULL);

    // Starts CivitWeb server
    mg_init_library(0);
    const struct mg_callbacks callbacks = {0};
    wrc->ctx = mg_start(&callbacks, wrc, (const char**) wrc->options.data);
    if (wrc->ctx == NULL) {
        WRC_LOGE("%s: error starting server", __func__);
        return NULL;
    }

    // Open internal zipped static filesystem, if configured.
    if (cfg->use_staticfs) {
        // Creates zip source from specified zip data and length
        zip_error_t error = {0};
        wrc->zip_src = zip_source_buffer_create(cfg->staticfs_data, cfg->staticfs_len, 0, &error);
        if (wrc->zip_src == NULL) {
            WRC_LOGE("%s: error creating zip source buffer", __func__);
            return NULL;
        }
        // Opens archive in source
        wrc->zip = zip_open_from_source(wrc->zip_src, ZIP_RDONLY|ZIP_CHECKCONS, &error);
        if (wrc->zip == NULL) {
            WRC_LOGE("%s: error opening zip staticfs", __func__);
            return NULL;
        }
        // Set CivitWeb request handler 
        mg_set_request_handler(wrc->ctx, "/*", wrc_zip_file_handler, wrc);
    }

    // Creates timer manager
    wrc->tm = cx_timer_create(wrc->alloc);
    if (wrc->tm == NULL) {
        WRC_LOGE("%s: error from cx_timer_create()", __func__);
        return NULL;
    }

    // Starts browser, if requested
    if (wrc->cfg.browser.start) {
        wrc_start_browser(wrc);
    }

    WRC_LOGD("%s: listening on: %d", __func__, wrc->used_port);
    WRC_LOGD("%s: using filesystem: %s", __func__, wrc->cfg.use_staticfs ? "INTERNAL" : "EXTERNAL");
    return wrc;
}


// Stops and destroy previously create WRC server
void  wrc_destroy(Wrc* wrc) {





}

void wrc_set_userdata(Wrc* wrc, void* userdata) {


}

void* wrc_get_userdata(Wrc* wrc) {




    return NULL;
}

//-----------------------------------------------------------------------------
// Local functions
//-----------------------------------------------------------------------------

static int wrc_find_port(Wrc* wrc) {

    const int min_port = 10000;
    const int max_port = 65000;
    srand(time(NULL));
    int retries = 50;
    while (retries-- >= 0) {
        // Get random port from the available interval
        int port = (rand() % (max_port + 1 - min_port)) + min_port;
        char test_port[32];
        snprintf(test_port, sizeof(test_port), "%u", port);

        // Try to start HTTP Server using this port
        const char* http_options[] = {
            "listening_ports", test_port,
            NULL, NULL
        };
        struct mg_callbacks http_callbacks = {0};
        struct mg_context* ctx = mg_start(&http_callbacks, 0, http_options);
        if (ctx == NULL) {
            continue;
        }
        // Port is available
        mg_stop(ctx);
        return port;
    }
    return -1;
}

static int wrc_zip_file_handler(struct mg_connection *conn, void *cbdata) {

    Wrc* wrc = cbdata;

    // Builds relative file path
    char filepath[256] = {0};
    if (wrc->cfg.staticfs_prefix) {
        strncpy(filepath, wrc->cfg.staticfs_prefix, sizeof(filepath));
    }
    const struct mg_request_info* rinfo = mg_get_request_info(conn);
    if (strcmp(rinfo->request_uri, "/") == 0) {
         strcat(filepath, "/index.html");
    } else {
        strcat(filepath, rinfo->request_uri);
    }

    // Locks access to the zip file
    assert(pthread_mutex_lock(&wrc->lock) == 0);
    int res = 0;

    // Get deflated file size from zip archive
    zip_stat_t stats;
    res = zip_stat(wrc->zip, filepath, 0, &stats);
    if (res) {
        mg_send_http_error(conn, 404, "%s", "Error: File not found");
        goto unlock;
    }

    // Allocates buffer to read file deflated data
    void *fileBuf = malloc(stats.size);
    if (fileBuf == NULL) {
        mg_send_http_error(conn, 500, "%s", "Error: No memory");
        res = 1;
        goto unlock;
    }

    // Opens zip file
    zip_file_t* zipf = zip_fopen(wrc->zip, filepath, 0);
    if (zipf == NULL) {
        mg_send_http_error(conn, 500, "%s", "Error: Open file");
        free(fileBuf);
        res = 1;
        goto unlock;
    }

    // Reads deflated file
    zip_int64_t nread = zip_fread(zipf, fileBuf, stats.size);
    if (nread < 0) {
        mg_send_http_error(conn, 500, "%s", "Error: Reading file");
        free(fileBuf);
        res = 1;
        goto unlock;
    }
    zip_fclose(zipf);

unlock:
    assert(pthread_mutex_unlock(&wrc->lock) == 0);
    if (res) {
        return res;
    }

    // Send headers
    res |= mg_response_header_start(conn, 200);
    const char* mime_type = mg_get_builtin_mime_type(filepath);
    res |= mg_response_header_add(conn, "Content-Type", mime_type, -1);
    char lenStr[64];
    snprintf(lenStr, sizeof(lenStr)-1, "%zu", stats.size);
    res |= mg_response_header_add(conn, "Content-Length", lenStr, -1);
    res |= mg_response_header_send(conn);
    assert(res == 0);

    // Send file data
    res = mg_write(conn, fileBuf, stats.size);
    assert(res == (int)stats.size);

    free(fileBuf);
    printf("zip:%s (%s)\n", filepath, mime_type);
    return res;
}

static int wrc_start_browser(Wrc* wrc) {

    // Generates URL
    char url[64];
    snprintf(url, sizeof(url), "http://localhost:%u", wrc->used_port);

    // Generates command line
    char command[1024];
    if (wrc->cfg.browser.standard) {
        snprintf(command, sizeof(command), "xdg-open \"%s\"", url);
    } else if (strlen(wrc->cfg.browser.cmd_line)) {
        snprintf(command, sizeof(command), "%s%s >>/dev/null 2>>/dev/null &", wrc->cfg.browser.cmd_line, url);
    }

    // Executes command
    WRC_LOGD("Starting browser:%s", command);
    int res = system(command);
    if (res < 0) {
        WRC_LOGE("Error starting browser:%s", command);
    }
    return res;
}


