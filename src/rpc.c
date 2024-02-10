#include <stddef.h>
#include <stdlib.h>
#include <time.h>

#include "cx_var.h"
#include "cx_pool_allocator.h"
#include "wrc.h"

// Local function binding info
typedef struct BindInfo {
    WrcRpcFn fn;
} BindInfo;

// Callback info
typedef struct ResponseInfo {
    WrcResponseFn   fn;     // Function to call when response arrives
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
    //WuiDecoder*             dec;            // Message decoder
    //WuiEncoder*             enc;            // Message encoder
    uint64_t                cid;            // Next call id
    map_resp                responses;      // Map of call cid to local callback function
} RpcClient;

// WebSocket handler state
typedef struct RpcHandler {
    Wrc*                wrc;            // Associated server
    const char*         url;            // This websocket handler URL
    uint32_t            max_conns;      // Maximum number of connection
    size_t              nconns;         // Current number of connections
    //arr_conn            conns;          // Array of connections info
    pthread_mutex_t     lock;           // Mutex for exclusive access to this struct
    //map_bind            binds;          // Map remote name to local bind info
    //WuiRpcEventCallback evcb;           // Optional user event callback
} RpcHandler;

WrcEndpoint* wrc_open_endpoint(Wrc* wrc, const char* url, size_t max_conns, WrcEventCallback cb) {


    return NULL;
}

void  wrc_close_endpoint(WrcEndpoint* ep) {


}

