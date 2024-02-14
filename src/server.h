#ifndef SERVER_H
#define SERVER_H

#include "civetweb.h"
#include "zip.h"
#include "wrs.h"

#include "cx_pool_allocator.h"
#include "cx_timer.h"

// Define/declare array of server options
#define cx_array_name arr_opt
#define cx_array_type const char*
#ifdef WRS_SERVER_IMPLEMENT
#   define cx_array_implement
#endif
#define cx_array_static
#include "cx_array.h"

// Define/declare hashmap from URL to WebSocket handler info
// NOTE: it must not be static as 'map_rpc' is also used by 'rpc.c'
#define cx_hmap_name                map_rpc
#define cx_hmap_key                 char*
#define cx_hmap_val                 WrsRpc*
#define cx_hmap_cmp_key(k1,k2,s)    strcmp(*(char**)k1,*(char**)k2)
#define cx_hmap_hash_key(k,s)       cx_hmap_hash_fnv1a32(*((char**)k), strlen(*(char**)k))
#define cx_hmap_free_key(k)         free(*k)
#ifdef WRS_SERVER_IMPLEMENT
#   define cx_hmap_implement
#endif
#include "cx_hmap.h"

// WRC server internal state
typedef struct Wrs {
    WrsConfig           cfg;            // Copy of user configuration
    arr_opt             options;        // Array of server options
    int                 used_port;      // Used TCP/IP listening port
    CxTimer*            tm;             // Timer manager
    pthread_mutex_t     lock;           // For exclusive access to this state
    struct mg_context*  ctx;            // CivitWeb context
    zip_source_t*       zip_src;        // For zip static filesystem
    zip_t*              zip;            // For zip static filesystem
    map_rpc             rpc_handlers;   // Map url to web socket rpc handler
    void*               userdata;       // Optional userdata
} Wrs;


#endif

