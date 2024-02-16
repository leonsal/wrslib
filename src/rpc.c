#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "cx_var.h"
#include "cx_alloc.h"
#include "cx_pool_allocator.h"

#include "wrs.h"
#include "server.h"
#include "rpc_codec.h"

// Local function binding info
typedef struct BindInfo {
    WrsRpcFn fn;
} BindInfo;

// Define internal hashmap from remote name to local rpc function
#define cx_hmap_name                map_bind
#define cx_hmap_key                 char*
#define cx_hmap_val                 BindInfo
#define cx_hmap_cmp_key(k1,k2,s)    strcmp(*(char**)k1,*(char**)k2)
#define cx_hmap_hash_key(k,s)       cx_hmap_hash_fnv1a32(*((char**)k), strlen(*(char**)k))
#define cx_hmap_free_key(k)         free(*k)
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
#define cx_hmap_implement
#define cx_hmap_static
#include "cx_hmap.h"

// State for each RPC client
typedef struct RpcClient {
    struct mg_connection*   conn;           // CivitWeb server WebSocket client connection
    CxVar*                  rxmsg;          // Received msg
    CxVar*                  txmsg;          // Response msg
    WrsDecoder*             dec;            // Message decoder
    WrsEncoder*             enc;            // Message encoder
    uint64_t                cid;            // Next call id
    map_resp                responses;      // Map of call cid to local callback function
} RpcClient;

// Define array of RPC client connections
#define cx_array_name arr_conn
#define cx_array_type RpcClient
#define cx_array_implement
#define cx_array_static
#include "cx_array.h"

// WebSocket RPC handler state
typedef struct WrsRpc {
    Wrs*                wrs;            // Associated server
    const char*         url;            // This websocket handler URL
    uint32_t            max_conns;      // Maximum number of connection
    size_t              nconns;         // Current number of connections
    arr_conn            conns;          // Array of connections info
    map_bind            binds;          // Map remote name to local bind info
    WrsEventCallback    evcb;           // Optional user event callback
    void*               userdata;       // Optional user data
} WrsRpc;


// Forward declaration of local functions
static int wrs_rpc_connect_handler(const struct mg_connection *conn, void *user_data);
static void wrs_rpc_ready_handler(struct mg_connection *conn, void *user_data);
static int wrs_rpc_data_handler(struct mg_connection *conn, int opcode, char *data, size_t dataSize, void *user_data);
static int wrs_rpc_call_handler(WrsRpc* rpc, RpcClient* client, size_t connid, const CxVar* rxmsg);
static int wrs_rpc_response_handler(WrsRpc* rpc, RpcClient* client, size_t connid, const CxVar* msg);
static void wrs_rpc_close_handler(const struct mg_connection *conn, void *user_data);

// Define websocket sub-protocols
// This must be static data, available between mg_start and mg_stop.
static const char subprotocol_bin[] = "Company.ProtoName.bin";
static const char subprotocol_json[] = "Company.ProtoName.json";
static const char *subprotocols[] = {subprotocol_bin, subprotocol_json, NULL};
static struct mg_websocket_subprotocols wsprot = {2, subprotocols};


WrsRpc* wrs_rpc_open(Wrs* wrs, const char* url, size_t max_conns, WrsEventCallback cb) {

    assert(pthread_mutex_lock(&wrs->lock) == 0);
    WrsRpc* handler = NULL;

    // Checks if there is already a handler for this url
    if (map_rpc_get(&wrs->rpc_handlers, (char*)url)) {
        goto exit;
    }

    // Creates and initializes the new RPC handler state
    handler = malloc(sizeof(WrsRpc));
    *handler = (WrsRpc) {
        .wrs = wrs,
        .url = url,
        .max_conns = max_conns,
        .conns = arr_conn_init(),
        .binds = map_bind_init(0),
        .evcb = cb,
    };

    // Save association of the url with new handler
    char* url_key = strdup(url);
    map_rpc_set(&wrs->rpc_handlers, url_key, handler);

    // Register the websocket callback functions.
    // mg_set_websocket_handler_with_subprotocols(wrs->ctx, url, &wsprot,
    //     wrs_rpc_connect_handler, wrs_rpc_ready_handler, wrs_rpc_data_handler, wrs_rpc_close_handler, handler);
    mg_set_websocket_handler(wrs->ctx, url,
        wrs_rpc_connect_handler, wrs_rpc_ready_handler, wrs_rpc_data_handler, wrs_rpc_close_handler, handler);

exit:
    assert(pthread_mutex_unlock(&wrs->lock) == 0);
    return handler;
}


void wrs_rpc_close(WrsRpc* rpc) {

    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);

    // Destroy all connections
    for (size_t i = 0; i < arr_conn_len(&rpc->conns); i++) {

    }
    // Destroy bindings
    map_bind_free(&rpc->binds);

    // Remove association of url with this RPC handler
    map_rpc_del(&rpc->wrs->rpc_handlers, (char*)rpc->url);

    // Remove WebSocket handler
    mg_set_websocket_handler_with_subprotocols(rpc->wrs->ctx, rpc->url, &wsprot, NULL, NULL, NULL, NULL, NULL);

    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
}

void wrs_rpc_set_userdata(WrsRpc* rpc, void* userdata) {

    rpc->userdata = userdata;
}

void* wrs_rpc_get_userdata(WrsRpc* rpc) {

    return rpc->userdata;
}

int wrs_rpc_bind(WrsRpc* rpc, const char* remote_name, WrsRpcFn fn) {

    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);
    int res = 0;

    // Checks if there is already an association in this handler with the specified remote name
    BindInfo* bind = map_bind_get(&rpc->binds, (char*)remote_name);
    if (bind) {
        res = 1;
        goto exit;
    }

    // Maps the remote name with the specified local function
    char* remote_name_key = strdup(remote_name);
    map_bind_set(&rpc->binds, remote_name_key, (BindInfo){.fn = fn});

exit:
    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
    return res;
}

int wrs_rpc_unbind(WrsRpc* rpc, const char* remote_name) {

    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);
    int res = 0;

    // Checks if there is already an association in this RPC endpoint with the specified remote name
    BindInfo* bind = map_bind_get(&rpc->binds, (char*)remote_name);
    if (bind == NULL) {
        res = 1;
        goto exit;
    }

    map_bind_del(&rpc->binds, (char*)remote_name);

exit:
    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
    return res;
}

CxVar* wrs_rpc_get_params(WrsRpc* rpc, size_t connid) {

    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);
    CxVar* params = NULL;

    // Checks if this connection id is valid
    if (connid >= arr_conn_len(&rpc->conns)) {
        WRS_LOGW("%s: connection:%zu is invalid", __func__, connid);
        goto exit;
    }

    params = cx_var_new(cxDefaultAllocator());

exit:
    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
    return params;
}


int wrs_rpc_call(WrsRpc* rpc, size_t connid, const char* remote_name, CxVar* params, WrsResponseFn cb) {

    int res = 0;
    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);

    // Checks if this connection id is valid
    if (connid >= arr_conn_len(&rpc->conns)) {
        WRS_LOGW("%s: connection:%zu is invalid", __func__, connid);
        assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
        return 1;
    }

    // Get the RPC client associated with this connection id and checks if it is active.
    RpcClient* client = &rpc->conns.data[connid];
    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
    if (client->conn == NULL) {
        WRS_LOGW("%s: connection:%zu closed with no associated client", __func__, connid);
        return 1;
    }
  
    // Sets the message envelope
    CxVar* msg = cx_var_new(cx_var_allocator(params));
    cx_var_set_map(msg);
    int64_t cid = client->cid;
    cx_var_set_map_int(msg, "cid", cid);
    cx_var_set_map_str(msg, "call", remote_name);
    cx_var_set_map_val(msg, "params", params);
    client->cid++;

    // Encodes message
    res = wrs_encoder_enc(client->enc, msg);
    if (res) {
        WRS_LOGE("%s: error encoding message", __func__);
        return 1;
    }

    // Get encoded message type and buffer
    bool text;
    size_t len;
    void* encoded = wrs_encoder_get_msg(client->enc, &text, &len);
    int opcode = text ? MG_WEBSOCKET_OPCODE_TEXT : MG_WEBSOCKET_OPCODE_BINARY;

    // Sends message to remote client
    mg_lock_connection((struct mg_connection*)client->conn);
    res = mg_websocket_write((struct mg_connection*)client->conn, opcode, encoded, len);
    mg_unlock_connection((struct mg_connection*)client->conn);
    if (res <= 0) {
        WRS_LOGE("%s: error:%d writing websocket message", __func__, res);
        return 1;
    }

    // If callback supplied, saves information to map response to the callback
    if (cb) {
        ResponseInfo rinfo = {.fn = cb };
        clock_gettime(CLOCK_REALTIME, &rinfo.time);
        map_resp_set(&client->responses, cid, rinfo);
    }

    return 0;  
}


WrsRpcInfo wrs_rpc_info(WrsRpc* rpc) {

    WrsRpcInfo info = {0};
    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);
    info.url = rpc->url;
    info.nconns = rpc->nconns;
    info.max_connid = arr_conn_len(&rpc->conns);
    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
    return info;
}

//-----------------------------------------------------------------------------
// Local functions
//-----------------------------------------------------------------------------


// Handler for new websocket connections
// The handler should return 0 to keep the WebSocket connection open
static int wrs_rpc_connect_handler(const struct mg_connection *conn, void *user_data) {

    WrsRpc* rpc = user_data;
    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);
    int res = 0;

    // If maximum number of connections reached, returns 1 to close the connection.
    if (rpc->nconns >= rpc->max_conns) {
        WRS_LOGW("%s: connection count exceeded for:%s", __func__, rpc->url);
        res = 1;
        goto exit;
    }

    // Create new RPC client state
    RpcClient new_client = {
        .conn = (struct mg_connection*)conn,
        .dec = wrs_decoder_new(cxDefaultAllocator()),
        .enc = wrs_encoder_new(cxDefaultAllocator()),
        .rxmsg = cx_var_new(cxDefaultAllocator()),
        .txmsg = cx_var_new(cxDefaultAllocator()),
        .cid = 100,
        .responses = map_resp_init(0),
    };

    // Looks for empty slot in the connections array
    // Note that the previous object 'out' and 'callbacks' will
    // be reused by the new connection.
    size_t connid = SIZE_MAX;
    for (size_t i = 0; i < arr_conn_len(&rpc->conns); i++) {
        if (rpc->conns.data[i].conn == NULL) {
            rpc->conns.data[i] = new_client;
            connid = i;
            break;
        }
    }

    // If empty slot not found, adds a new RpcClient to connections array
    if (connid == SIZE_MAX) {
        arr_conn_push(&rpc->conns, new_client);
        connid = arr_conn_len(&rpc->conns)-1;
    }
    rpc->nconns++;
    mg_set_user_connection_data(conn, (void*)(connid));

exit:
    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
    // Calls user handler if defined and if no errors occurred.
    if (res == 0 && rpc->evcb) {
        rpc->evcb(rpc, connid, WrsEventOpen);
    }
    return res;
}

// Handler indicating the connection is ready to receive data.
static void wrs_rpc_ready_handler(struct mg_connection *conn, void *user_data) {

    // Calls user handler
    WrsRpc* rpc = user_data;
    const uintptr_t connid = (uintptr_t)mg_get_user_connection_data(conn);
    if (rpc->evcb) {
        rpc->evcb(rpc, connid, WrsEventReady);
    }
}

// Handler called when RPC message received.
// The message could be a remote call from the client or a response to a previous call from the server.
// The handler should return 1 to keep the WebSocket connection open or 0 to close it.
static int wrs_rpc_data_handler(struct mg_connection *conn, int opcode, char *data, size_t dataSize, void *user_data) {

    WRS_LOGD("%s: data size:%zu", __func__, dataSize);

    WrsRpc* rpc = user_data;
    uintptr_t connid = (uintptr_t)mg_get_user_connection_data(conn);
    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);

    // Checks connection id and closes connection if invalid.
    if (connid >= arr_conn_len(&rpc->conns)) {
        WRS_LOGW("%s: message received with invalid connid:%zu", __func__, connid);
        assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
        return 0;
    }

    // Get the RPC client associated with this connection and
    // closes connection if client is not opened.
    RpcClient* client = &rpc->conns.data[connid];
    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
    if (client->conn == NULL) {
        WRS_LOGW("%s: message received for closed connid:%zu", __func__, connid);
        return 0;
    }

    // Accepts text or binary messages only.
    // Closes connection if client is not opened.
    bool text = true;
    if ((opcode & 0xF) == MG_WEBSOCKET_OPCODE_TEXT) {
        ;
    } else if ((opcode & 0xF) == MG_WEBSOCKET_OPCODE_BINARY) {
        text = false;
    } else {
        WRS_LOGW("%s: WebSocket msg type:%d ignored", __func__, opcode & 0xF);
        return 0;
    }

    // Decodes message and closes connection if invalid
    int res = wrs_decoder_dec(client->dec, text, data, dataSize, client->rxmsg);
    if (res) {
        WRS_LOGE("%s: received invalid message", __func__);
        return 0;
    }

    // Try to process this message as remote call
    res = wrs_rpc_call_handler(rpc, client, connid, client->rxmsg);
    if (res == 0) {
        return 1;
    }
    // Try to process this message as response from previous local call.
    if (res == 1) {
        res = wrs_rpc_response_handler(rpc, client, connid, client->rxmsg);
        if (res == 0) {
            return 1;
        }
    }
    // Close connection
    return 0;
}

// Called by RPC data handler to process remote calls.
// Returns 0 if OK
// Returns 1 if not a call handler (cid undefined)
// Returns 2 for other errors
static int wrs_rpc_call_handler(WrsRpc* rpc, RpcClient* client, size_t connid, const CxVar* rxmsg) {

    // Checks message fields for remote call:
    // cid:     <number>
    // call:    <string>
    // params:  <any>
    int64_t cid;
    if (!cx_var_get_map_int(rxmsg, "cid", &cid)) {
        return 1;
    }
    const char* pcall;
    if (!cx_var_get_map_str(rxmsg, "call", &pcall)) {
        WRS_LOGE("%s: 'call' field not found", __func__);
        return 2;
    }
    CxVar* params = cx_var_get_map_val(rxmsg, "params");
    if (!params) {
        WRS_LOGE("%s: 'params' field not found", __func__);
        return 2;
    }

    // Get local function binding for the received "call"
    BindInfo* rinfo = map_bind_get(&rpc->binds, (char*)pcall);
    if (rinfo == NULL) {
        WRS_LOGE("%s: bind for:%s not found", __func__, pcall);
        return 2;
    }

    // Prepare response
    cx_var_set_map(client->txmsg);
    cx_var_set_map_int(client->txmsg, "rid", cid);
    CxVar* resp = cx_var_set_map_map(client->txmsg, "resp");

    // Calls local function and if it returns error,
    // does not send any response to remote caller.
    int res = rinfo->fn(rpc, connid, params, resp);
    if (res) {
        WRS_LOGW("%s: local rpc function returned error", __func__);
        return 0;
    }

    // Encodes message
    res = wrs_encoder_enc(client->enc, client->txmsg);
    if (res) {
        WRS_LOGE("%s: error encoding message", __func__);
        return 0;
    }

    // Get encoded message type and buffer
    bool text;
    size_t len;
    void* msg = wrs_encoder_get_msg(client->enc, &text, &len);
    int opcode = text ? MG_WEBSOCKET_OPCODE_TEXT : MG_WEBSOCKET_OPCODE_BINARY;

    // Sends response to remote client
    mg_lock_connection((struct mg_connection*)client->conn);
    res = mg_websocket_write((struct mg_connection*)client->conn, opcode, msg, len);
    mg_unlock_connection((struct mg_connection*)client->conn);
    if (res <= 0) {
        WRS_LOGE("%s: error:%d writing websocket message", __func__, res);
    }
    return 0;
}

// Called by RPC data handler to process received possible response
// Returns 0 if OK
// Returns 1 for other errors
static int wrs_rpc_response_handler(WrsRpc* rpc, RpcClient* client, size_t connid, const CxVar* msg) {

    // The response message body must have the following format:
    // { rid: <number>, resp: {err: <any> OR data: <any>}}
    int64_t rid;
    if (!cx_var_get_map_int(msg, "rid", &rid)) {
        WRS_LOGE("%s: response with missing 'rid' field", __func__);
        return 1;
    }

    // Get the response field
    CxVar* resp = cx_var_get_map_val(msg, "resp");
    if (resp == NULL) {
        WRS_LOGE("%s: response with missing 'resp' field", __func__);
        return 1;
    }
    
    // Get information for the local callback for this response
    ResponseInfo* info = map_resp_get(&client->responses, rid);
    if (info == NULL) {
        WRS_LOGE("%s: response with no callback connid:%zu rid:%zu", __func__, connid, rid);
        return 1;
    }

    // Removes response callback association and calls response callback
    // The response callback should return 0 to keep the connection open.
    map_resp_del(&client->responses, rid);
    return info->fn(rpc, connid, resp);
}

// Handler called when RPC client connection is closed.
static void wrs_rpc_close_handler(const struct mg_connection *conn, void *user_data) {

    WrsRpc* rpc = user_data;
    const uintptr_t connid = (uintptr_t)mg_get_user_connection_data(conn);
    int res = 0;

    // Checks if this connection id is valid
    assert(pthread_mutex_lock(&rpc->wrs->lock) == 0);
    if (connid >= arr_conn_len(&rpc->conns)) {
        WRS_LOGW("%s: connection:%zu is invalid", __func__, connid);
        res = 1;
        goto exit;
    }

    // Get the RPC client associated with this connection id and checks if it is active.
    RpcClient* client = &rpc->conns.data[connid];
    if (client->conn == NULL) {
        WRS_LOGW("%s: connection:%zu closed with no associated client", __func__, connid);
        res = 1;
        goto exit;
    }

    // Deallocates all memory used by this client connection and clears the 
    // slot in the connections array, which can be reused for new connections.
    memset(client, 0, sizeof(RpcClient));
    rpc->nconns--;

exit:
    assert(pthread_mutex_unlock(&rpc->wrs->lock) == 0);
    // Calls user handler if defined and if no errors occurred.
    if (res == 0 && rpc->evcb) {
        rpc->evcb(rpc, connid, WrsEventClose);
    }
}


