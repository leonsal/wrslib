#ifndef RPC_CODEC_H
#define RPC_CODEC_H

#include <stdbool.h>
#include <time.h>

#include "cx_alloc.h"
#include "cx_var.h"

// Encoded binary message chunk types
typedef enum {
    WuiChunkMsg = 1,
    WuiChunkBuf,
    WuiChunkTypeInvalid,
} WuiChunkType;

// Creates encoder for messages buffer using specified allocator.
typedef struct RpcEncoder RpcEncoder;
RpcEncoder* rpc_encoder_new(const CxAllocator* alloc);

// Destroy previously created encoder, deallocating memory
void rpc_encoder_del(RpcEncoder*);

// Clear encoder buffer, without deallocating memory
void rpc_encoder_clear(RpcEncoder* e);

// Encodes message into internal buffer and returns non-zero error.
int rpc_encoder_enc(RpcEncoder* e, CxVar* msg);

// Get last message encoded
void* rpc_encoder_msg(RpcEncoder* e, bool* text, size_t* len);

//-----------------------------------------------------------------------------
// Decoder
//-----------------------------------------------------------------------------

// Creates message decoder
typedef struct RpcDecoder RpcDecoder;
RpcDecoder* rpc_decoder_new(const CxAllocator* alloc);

// Destroy previously created decoder, deallocating memory
void rpc_decoder_del(RpcDecoder* d);

// Clear decoder state, without deallocating memory
void rpc_decoder_clear(RpcDecoder* e);

// Decodes message and returns non zero error code if message is invalid
int rpc_decoder_dec(RpcDecoder* d, bool text, void* data, size_t len, CxVar* msg);


#endif

