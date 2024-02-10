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
typedef struct WrsEncoder WrsEncoder;
WrsEncoder* wrs_encoder_new(const CxAllocator* alloc);

// Destroy previously created encoder, deallocating memory
void wrs_encoder_del(WrsEncoder*);

// Clear encoder buffer, without deallocating memory
void wrs_encoder_clear(WrsEncoder* e);

// Encodes message into internal buffer and returns non-zero error.
int wrs_encoder_enc(WrsEncoder* e, CxVar* msg);

// Get last message encoded
void* wrs_encoder_msg(WrsEncoder* e, bool* text, size_t* len);

//-----------------------------------------------------------------------------
// Decoder
//-----------------------------------------------------------------------------

// Creates message decoder
typedef struct WrsDecoder WrsDecoder;
WrsDecoder* wrs_decoder_new(const CxAllocator* alloc);

// Destroy previously created decoder, deallocating memory
void wrs_decoder_del(WrsDecoder* d);

// Clear decoder state, without deallocating memory
void wrs_decoder_clear(WrsDecoder* e);

// Decodes message and returns non zero error code if message is invalid
int wrs_decoder_dec(WrsDecoder* d, bool text, void* data, size_t len, CxVar* msg);


#endif

