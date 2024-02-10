#ifndef WRC_H
#define WRC_H

// Declare or define WRC logger
#define cx_log_name wrc_logger
#define cx_log_tsafe
#ifdef WRC_LOG_IMPLEMENT
#   define cx_log_implement
#endif
#include "cx_log.h"
#ifndef WRC_LOG_IMPLEMENT
extern wrc_logger wrc_default_logger;
#endif

// Logger utility macros
#define WRC_LOGD(...) wrc_logger_deb(&wrc_default_logger, __VA_ARGS__)
#define WRC_LOGI(...) wrc_logger_info(&wrc_default_logger, __VA_ARGS__)
#define WRC_LOGW(...) wrc_logger_warn(&wrc_default_logger, __VA_ARGS__)
#define WRC_LOGE(...) wrc_logger_error(&wrc_default_logger, __VA_ARGS__)
#define WRC_LOGF(...) wrc_logger_fatal(&wrc_default_logger, __VA_ARGS__)

#include "cx_var.h"

// Error codes
typedef enum {
    WrcErrorNoMem = 1,
    WrcErrorServerStart,
    WrcErrorZip,
    WrcErrorRpcEndpointExist,
    WrcErrorRpcEndpointNotExist,
} WrcError;

// WRC Configuration
typedef struct WrcConfig {
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
} WrcConfig;

// Creates and starts WRC server with the specified configuration.
// Returns NULL on error.
typedef struct Wrc Wrc;
Wrc* wrc_create(const WrcConfig* cfg);

// Stops and destroy previously create WRC server
void wrc_destroy(Wrc* wrc); 

// Sets user data associated with this server
void wrc_set_userdata(Wrc* wrc, void* userdata); 

// Get previously set userdata associated with server.
void* wrc_get_userdata(Wrc* wrc);

// Type for local C functions called by remote clients
// url - identifies the endpoint which received the message
// connid - identifies the connection id for the endpoint
// params - call parameters
// resp - optional response data
// Must return 0 to allow response to be sent back to remote caller.
typedef int (*WrcRpcFn)(Wrc* wrc, const char* url, size_t connid, CxVar* params, CxVar* resp);

// Event types
typedef enum {
    WrcEventOpen,     // RPC endpoint opened
    WrcEventClose,    // RPC endpoint closed
    WrcEventReady,    // RPC endpoint ready to send
} WrcEvent;

// Type for local callback function to receive events
typedef void (*WrcEventCallback)(Wrc* wrc, const char* url, size_t connid, WrcEvent ev);

// Open RPC endpoint
// wrc - WRC server object
// url - Relative url for this endpoint
// max_conns - Maximum number of client connections
// vcb - Optional callback to receive events
// returns NULL pointer on error.
typedef struct WrcEndpoint WrcEndpoint;
WrcEndpoint* wrc_open_endpoint(Wrc* wrc, const char* url, size_t max_conns, WrcEventCallback cb);

// Close previously created RPC endpoint
// ep - Pointer to endpoint
// returns non zero error code on errors.
void  wrc_close_endpoint(WrcEndpoint* ep);

// Binds a local C function to be called by remote client using RPC.
// ep - Wrc endpoint
// remote_name - the name used by remote client to call this local function.
// fn - pointer to local function which will be called.
// Returns non zero result on errors.
int wrc_bind(WrcEndpoint* ep, const char* remote_name, WrcRpcFn fn);

// Unbinds a previously binded local function from the endpoint
// ep - Wrc endpoint
// remote_name - the name used by remote client to call this local function.
int wrc_unbind(WrcEndpoint* ep, const char* remote_name);

// Type for RPC response function
// ep - endpoint from which the response arrived
// url - identifies the RPC endpoint
// connid - identifies the connection id
// resp - response message
typedef int (*WrcResponseFn)(WrcEndpoint* ep, size_t connid, const CxVar* resp);

// Calls remote function using RPC connection
// url - identifies the RPC endpoint
// connid - identifies the specific connection for this endpoint
// remote_name - the name of the remote function to call
// params - message with parameters to send to remote function
// cb - Optional callback to receive response from remote function
int wrc_rpc_call(WrcEndpoint* ep, size_t connid, const char* remote_name, CxVar* params, WrcResponseFn cb);

// Returns information about specified RPC endpoint
typedef struct WrcEndpointInfo {
    size_t  nconns;         // Current number of connection
    size_t  max_connid;     // Maximum valid connection id
} WrcEndpointInfo;
WrcEndpointInfo wrc_get_endpoint_info(WrcEndpoint* ep);








#endif

