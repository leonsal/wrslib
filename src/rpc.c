#include <stddef.h>
#include <stdlib.h>
#include <time.h>

#include "cx_var.h"
#include "cx_pool_allocator.h"
#include "wrs.h"
#include "rpc_codec.h"

// Local function binding info
typedef struct BindInfo {
    WrsRpcFn fn;
} BindInfo;

// Define internal hashmap from remote name to local rpc function
#define cx_hmap_name map_bind
#define cx_hmap_key  const char*
#define cx_hmap_val  BindInfo
#define cx_hmap_cmp_key cx_hmap_cmp_key_str_ptr
#define cx_hmap_hash_key cx_hmap_hash_key_str_ptr
#define cx_hmap_instance_allocator
#define cx_hmap_implement
#define cx_hmap_static
#include "cx_hmap.h"

// Callback info
typedef struct ResponseInfo {
    WrsResponseFn   fn;     // Function to call when response arrives
    struct timespec time;   // Time when call was sent to client
} ResponseInfo;

// Define map of call id to local rpc callback function pointer
#define cx_hmap_name map_resp
#define cx_hmap_key  uint64_t
#define cx_hmap_val  ResponseInfo
#define cx_hmap_instance_allocator
#define cx_hmap_implement
#define cx_hmap_static
#include "cx_hmap.h"

// State for each RPC client
typedef struct RpcClient {
    struct mg_connection*   conn;           // CivitWeb server WebSocket client connection
    CxPoolAllocator*        pa;             // Pool allocator
    const CxAllocator*      alloc;          // Allocator interface for Pool Allocator
    CxVar*                  rxmsg;            
    CxVar*                  txmsg;
    WrsDecoder*             dec;            // Message decoder
    WrsEncoder*             enc;            // Message encoder
    uint64_t                cid;            // Next call id
    map_resp                responses;      // Map of call cid to local callback function
} RpcClient;

// Define internal array of RPC client connections
#define cx_array_name arr_conn
#define cx_array_type RpcClient
#define cx_array_instance_allocator
#define cx_array_implement
#define cx_array_static
#include "cx_array.h"

// WebSocket RPC handler state
typedef struct RpcHandler {
    Wrs*                wrc;            // Associated server
    const char*         url;            // This websocket handler URL
    uint32_t            max_conns;      // Maximum number of connection
    size_t              nconns;         // Current number of connections
    arr_conn            conns;          // Array of connections info
    pthread_mutex_t     lock;           // Mutex for exclusive access to this struct
    map_bind            binds;          // Map remote name to local bind info
    WrsEventCallback    evcb;           // Optional user event callback
} RpcHandler;

WrsEndpoint* wrc_open_endpoint(Wrs* wrc, const char* url, size_t max_conns, WrsEventCallback cb) {


    return NULL;
}

void  wrc_close_endpoint(WrsEndpoint* ep) {


}

