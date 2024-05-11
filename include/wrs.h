#ifndef WRS_H
#define WRS_H

#include "cx_logger.h"
// Logger utility macros
extern CxLogger* wrs_default_logger;
#define WRS_LOGD(...) cx_logger_log(wrs_default_logger, CxLoggerDebug, __VA_ARGS__)
#define WRS_LOGI(...) cx_logger_log(wrs_default_logger, CxLoggerInfo, __VA_ARGS__)
#define WRS_LOGW(...) cx_logger_log(wrs_default_logger, CxLoggerWarn, __VA_ARGS__)
#define WRS_LOGE(...) cx_logger_log(wrs_default_logger, CxLoggerError, __VA_ARGS__)
#define WRS_LOGF(...) cx_logger_log(wrs_default_logger, CxLoggerFatal, __VA_ARGS__)

#include "cx_error.h"
#include "cx_var.h"

// Error codes
typedef enum {
    WrsErrorNoMem = 1,
    WrsErrorServerStart,
    WrsErrorZip,
    WrsErrorRpcEndpointExist,
    WrsErrorRpcEndpointNotExist,
} WrsError;

// WRS Configuration
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

// Creates and starts WRS server with the specified configuration.
// Returns NULL on error.
typedef struct Wrs Wrs;
Wrs* wrs_create(const WrsConfig* cfg);

// Stops and destroy previously create WRS server
void wrs_destroy(Wrs* wrs); 


// Type for local C functions called by remote clients
// rpc - pointer to RPC endpoint which received the message
// connid - the connection id for the endpoint
// params - call parameters
// resp - optional response data
// Must return 0 to allow response to be sent back to remote caller.
typedef struct WrsRpc WrsRpc;
typedef int (*WrsRpcFn)(WrsRpc* rpc, size_t connid, CxVar* params, CxVar* resp);

// Event types
typedef enum {
    WrsEventOpen,     // RPC endpoint opened
    WrsEventClose,    // RPC endpoint closed
    WrsEventReady,    // RPC endpoint ready to send
} WrsEvent;

// Type for local callback function to receive events
typedef void (*WrsEventCallback)(WrsRpc* rpc, size_t connid, WrsEvent ev);

// Open RPC endpoint and returns its pointer
// wrs - WRS server
// url - Relative url for this endpoint
// max_conns - Maximum number of client connections
// cb - Optional callback to receive events for this endpoint
// Returns NULL pointer on error.
WrsRpc* wrs_rpc_open(Wrs* wrs, const char* url, size_t max_conns, WrsEventCallback cb);

// Close previously created RPC endpoint
// rpc - Pointer to the RPC endpoint
// Returns non zero error code on errors.
void  wrs_rpc_close(WrsRpc* rpc);

// Sets user data associated with this RPC endpoint
void wrs_rpc_set_userdata(WrsRpc* rpc, void* userdata); 

// Get previously set userdata associated with this RPC endpoint
void* wrs_rpc_get_userdata(WrsRpc* rpc);

// Binds a local C function to be called by remote client using RPC.
// Only one function can be binded to a remote name.
// rpc - RPC endpoint
// remote_name - the name used by remote client to call this local function.
// local_fn - pointer to local function which will be called.
CxError wrs_rpc_bind(WrsRpc* rpc, const char* remote_name, WrsRpcFn local_fn);

// Unbinds a previously binded local function from the endpoint
// rpc - RPC endpoint
// remote_name - the name used by remote client to call this local function.
CxError wrs_rpc_unbind(WrsRpc* rpc, const char* remote_name);

// Type for RPC response function
// rpc - RPC endpoint from which the response arrived
// connid - identifies the connection id
// resp - response message
// Returns zero to keep the connection open
typedef int (*WrsResponseFn)(WrsRpc* rpc, size_t connid, CxVar* resp);

// Calls remote function using RPC connection
// url - identifies the RPC endpoint
// connid - identifies the specific connection for this endpoint
// remote_name - the name of the remote function to call
// params - message with parameters to send to remote function
//  this should be allocated by the user, but is deallocated by this function
// cb - Optional callback to receive response from remote function
// Returns non zero value on errors.
// After the function returns, the 'params' CxVar may be destroyed.
CxError wrs_rpc_call(WrsRpc* rpc, size_t connid, const char* remote_name, CxVar* params, WrsResponseFn cb);

// Returns information about specified RPC endpoint
typedef struct WrsRpcInfo {
    const char* url;        // Associated url
    size_t  nconns;         // Current number of connection
    size_t  max_connid;     // Maximum valid connection id
} WrsRpcInfo;
WrsRpcInfo wrs_rpc_info(WrsRpc* rpc);

#endif

