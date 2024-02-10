#ifndef WRS_H
#define WRS_H

// Declare or define WRS logger
#define cx_log_name wrs_logger
#define cx_log_tsafe
#ifdef WRS_LOG_IMPLEMENT
#   define cx_log_implement
#endif
#include "cx_log.h"
#ifndef WRS_LOG_IMPLEMENT
extern wrs_logger wrs_default_logger;
#endif

// Logger utility macros
#define WRS_LOGD(...) wrs_logger_deb(&wrs_default_logger, __VA_ARGS__)
#define WRS_LOGI(...) wrs_logger_info(&wrs_default_logger, __VA_ARGS__)
#define WRS_LOGW(...) wrs_logger_warn(&wrs_default_logger, __VA_ARGS__)
#define WRS_LOGE(...) wrs_logger_error(&wrs_default_logger, __VA_ARGS__)
#define WRS_LOGF(...) wrs_logger_fatal(&wrs_default_logger, __VA_ARGS__)

#include "cx_var.h"

// Error codes
typedef enum {
    WrsErrorNoMem = 1,
    WrsErrorServerStart,
    WrsErrorZip,
    WrsErrorRpcEndpointExist,
    WrsErrorRpcEndpointNotExist,
} WrsError;

// WRC Configuration
typedef struct WrsConfig {
    char*       document_root;          // Document root path
    int         listening_port;         // HTTP server listening port (0 for auto port)
    bool        use_staticfs;           // Use internal embedded static filesystem (zip)                                       
    char*       staticfs_prefix;        // Static filesystem (zip) prefix
    const void* staticfs_data;          // Pointer to static filesystem zip data
    size_t      staticfs_len;           // Length in bytes of static filesystem data                            
    struct {
        bool    start;                  // Starts browser after server started
        bool    standard;               // Use standard (default) browser or:
        char    cmd_line[512];          // Browser command line without URL
    } browser;
} WrsConfig;

// Creates and starts WRC server with the specified configuration.
// Returns NULL on error.
typedef struct Wrs Wrs;
Wrs* wrs_create(const WrsConfig* cfg);

// Stops and destroy previously create WRC server
void wrs_destroy(Wrs* wrc); 

// Sets user data associated with this server
void wrs_set_userdata(Wrs* wrc, void* userdata); 

// Get previously set userdata associated with server.
void* wrs_get_userdata(Wrs* wrc);

// Type for local C functions called by remote clients
// url - identifies the endpoint which received the message
// connid - identifies the connection id for the endpoint
// params - call parameters
// resp - optional response data
// Must return 0 to allow response to be sent back to remote caller.
typedef int (*WrsRpcFn)(Wrs* wrc, const char* url, size_t connid, CxVar* params, CxVar* resp);

// Event types
typedef enum {
    WrsEventOpen,     // RPC endpoint opened
    WrsEventClose,    // RPC endpoint closed
    WrsEventReady,    // RPC endpoint ready to send
} WrsEvent;

// Type for local callback function to receive events
typedef struct WrsRpc WrsRpc;
typedef void (*WrsEventCallback)(WrsRpc* rpc, size_t connid, WrsEvent ev);

// Open RPC endpoint
// wrs - WRS server
// url - Relative url for this endpoint
// max_conns - Maximum number of client connections
// vcb - Optional callback to receive events
// returns NULL pointer on error.
WrsRpc* wrs_rpc_open(Wrs* wrs, const char* url, size_t max_conns, WrsEventCallback cb);

// Close previously created RPC endpoint
// ep - Pointer to endpoint
// returns non zero error code on errors.
void  wrs_rpc_close(WrsRpc* ep);

// Binds a local C function to be called by remote client using RPC.
// ep - Wrs endpoint
// remote_name - the name used by remote client to call this local function.
// fn - pointer to local function which will be called.
// Returns non zero result on errors.
int wrs_rpc_bind(WrsRpc* ep, const char* remote_name, WrsRpcFn local_fn);

// Unbinds a previously binded local function from the endpoint
// ep - Wrs endpoint
// remote_name - the name used by remote client to call this local function.
int wrs_rpc_unbind(WrsRpc* ep, const char* remote_name);

// Type for RPC response function
// ep - endpoint from which the response arrived
// url - identifies the RPC endpoint
// connid - identifies the connection id
// resp - response message
typedef int (*WrsResponseFn)(WrsRpc* ep, size_t connid, const CxVar* resp);

// Calls remote function using RPC connection
// url - identifies the RPC endpoint
// connid - identifies the specific connection for this endpoint
// remote_name - the name of the remote function to call
// params - message with parameters to send to remote function
// cb - Optional callback to receive response from remote function
int wrs_rpc_call(WrsRpc* ep, size_t connid, const char* remote_name, CxVar* params, WrsResponseFn cb);

// Returns information about specified RPC endpoint
typedef struct WrsRpcInfo {
    const char* url;        // Associated url
    size_t  nconns;         // Current number of connection
    size_t  max_connid;     // Maximum valid connection id
} WrsRpcInfo;
WrsRpcInfo wrs_rpc_info(WrsRpc* rpc);








#endif

