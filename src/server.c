#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "civetweb.h"
#include "zip.h"

#define WRS_LOG_IMPLEMENT
#include "wrs.h"

#define WRS_SERVER_IMPLEMENT
#include "server.h"

// Global logger (can be used by dependants)
wrs_logger wrs_default_logger;

// Forward declarations of local functions
static int wrs_find_port(Wrs* wrs);
static int wrs_zip_file_handler(struct mg_connection *conn, void *cbdata);
static int wrs_start_browser(Wrs* wrs);


Wrs* wrs_create(const WrsConfig* cfg) {

    // Creates and initializes internal state
    Wrs* wrs = malloc(sizeof(Wrs));
    if (wrs == NULL) {
        return NULL;
    }
    memset(wrs, 0, sizeof(Wrs));
    wrs->cfg = *cfg;
    wrs->rpc_handlers = map_rpc_init(0);
    assert(pthread_mutex_init(&wrs->lock, NULL) == 0);

    // If configured listening port is 0, finds an unused port
    wrs->used_port = cfg->listening_port;
    if (cfg->listening_port == 0) {
        int port = wrs_find_port(wrs);
        if (port < 0) {
            return NULL;
        }
        wrs->used_port = port;
    }

    // Builds server options array
    wrs->options = arr_opt_init();
    if (cfg->document_root) {
        arr_opt_push(&wrs->options, "document_root");
        char* document_root = strdup(cfg->document_root);
        strcpy(document_root, cfg->document_root);
        arr_opt_push(&wrs->options, document_root);
    }
    // Sets listening port
    const size_t size = 32;
    char* listening_ports = malloc(size);
    snprintf(listening_ports, size-1, "%u", wrs->used_port);
    arr_opt_push(&wrs->options, "listening_ports");
    arr_opt_push(&wrs->options, listening_ports);
    // Options array terminator
    arr_opt_push(&wrs->options, NULL);
    arr_opt_push(&wrs->options, NULL);

    // Starts CivitWeb server
    mg_init_library(0);
    const struct mg_callbacks callbacks = {0};
    wrs->ctx = mg_start(&callbacks, wrs, (const char**) wrs->options.data);

    // Free options array allocated elements (odd indexes)
    for (size_t i = 0; i < arr_opt_len(&wrs->options); i++) {
        if (i % 2) {
            free((char*)wrs->options.data[i]);
        }
    }
    arr_opt_free(&wrs->options);

    // Checks server error
    if (wrs->ctx == NULL) {
        WRS_LOGE("%s: error starting server", __func__);
        return NULL;
    }

    // Open internal zipped static filesystem, if configured.
    if (cfg->use_staticfs) {
        // Creates zip source from specified zip data and length
        zip_error_t error = {0};
        wrs->zip_src = zip_source_buffer_create(cfg->staticfs_data, cfg->staticfs_len, 0, &error);
        if (wrs->zip_src == NULL) {
            WRS_LOGE("%s: error creating zip source buffer", __func__);
            return NULL;
        }
        // Opens archive in source
        wrs->zip = zip_open_from_source(wrs->zip_src, ZIP_RDONLY|ZIP_CHECKCONS, &error);
        if (wrs->zip == NULL) {
            WRS_LOGE("%s: error opening zip staticfs", __func__);
            return NULL;
        }
        // Set CivitWeb request handler 
        mg_set_request_handler(wrs->ctx, "/*", wrs_zip_file_handler, wrs);
    }

    // Creates timer manager
    wrs->tm = cx_timer_create(cxDefaultAllocator());
    if (wrs->tm == NULL) {
        WRS_LOGE("%s: error from cx_timer_create()", __func__);
        return NULL;
    }

    // Starts browser, if requested
    if (wrs->cfg.browser.start) {
        wrs_start_browser(wrs);
    }

    WRS_LOGD("%s: listening on: %d", __func__, wrs->used_port);
    WRS_LOGD("%s: using filesystem: %s", __func__, wrs->cfg.use_staticfs ? "INTERNAL" : "EXTERNAL");
    return wrs;
}


// Stops and destroy previously create wrs server
void  wrs_destroy(Wrs* wrs) {

    mg_stop(wrs->ctx);
    wrs->ctx = NULL;

    // Destroy all rpc endpoints
    // NOTE: wrs_rpc_close() deletes the its map entry so the loop must
    // initialize the iterator each time.
    while (true) {
        map_rpc_iter iter = {0};
        map_rpc_entry* e = map_rpc_next(&wrs->rpc_handlers, &iter);
        if (e == NULL) {
            break;
        }
        wrs_rpc_close(e->val);
    }
    map_rpc_free(&wrs->rpc_handlers);

    cx_timer_destroy(wrs->tm);
    if (wrs->zip) {
        zip_close(wrs->zip);
    }
    assert(pthread_mutex_destroy(&wrs->lock) == 0);
    free(wrs);
}

//-----------------------------------------------------------------------------
// Local functions
//-----------------------------------------------------------------------------

static int wrs_find_port(Wrs* wrs) {

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

static int wrs_zip_file_handler(struct mg_connection *conn, void *cbdata) {

    Wrs* wrs = cbdata;

    // Builds relative file path
    char filepath[256] = {0};
    if (wrs->cfg.staticfs_prefix) {
        strncpy(filepath, wrs->cfg.staticfs_prefix, sizeof(filepath));
    }
    const struct mg_request_info* rinfo = mg_get_request_info(conn);
    if (strcmp(rinfo->request_uri, "/") == 0) {
         strcat(filepath, "/index.html");
    } else {
        strcat(filepath, rinfo->request_uri);
    }

    // Locks access to the zip file
    assert(pthread_mutex_lock(&wrs->lock) == 0);
    int res = 0;

    // Get deflated file size from zip archive
    zip_stat_t stats;
    res = zip_stat(wrs->zip, filepath, 0, &stats);
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
    zip_file_t* zipf = zip_fopen(wrs->zip, filepath, 0);
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
    assert(pthread_mutex_unlock(&wrs->lock) == 0);
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

static int wrs_start_browser(Wrs* wrs) {

    // Generates URL
    char url[64];
    snprintf(url, sizeof(url), "http://localhost:%u", wrs->used_port);

    // Generates command line
    char command[1024];
    if (wrs->cfg.browser.standard) {
        snprintf(command, sizeof(command), "xdg-open \"%s\"", url);
    } else if (strlen(wrs->cfg.browser.cmd_line)) {
        snprintf(command, sizeof(command), "%s%s >>/dev/null 2>>/dev/null &", wrs->cfg.browser.cmd_line, url);
    }

    // Executes command
    WRS_LOGD("Starting browser:%s", command);
    int res = system(command);
    if (res < 0) {
        WRS_LOGE("Error starting browser:%s", command);
    }
    return res;
}


