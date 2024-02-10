#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#include "civetweb.h"
#include "cx_alloc.h"
#include "cx_pool_allocator.h"
#include "zip.h"

#include "cx_timer.h"
#define WRC_LOG_IMPLEMENT
#include "wrc.h"
// Static logger (can be used by dependants)
wrc_logger wrc_default_logger;

// WRC server internal state
typedef struct Wrc {
    WrcConfig           cfg;            // Copy of user configuration
    CxPoolAllocator*    pool_alloc;     // Pool allocator
    const CxAllocator*  alloc;          // Allocator interface for Pool Allocator
    //arr_opt             options;        // Array of server options
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

    return 0;
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

