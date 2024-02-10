#ifndef wrc_CODEC_H
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
typedef struct WrcEncoder WrcEncoder;
WrcEncoder* wrc_encoder_new(const CxAllocator* alloc);

// Destroy previously created encoder, deallocating memory
void wrc_encoder_del(WrcEncoder*);

// Clear encoder buffer, without deallocating memory
void wrc_encoder_clear(WrcEncoder* e);

// Encodes message into internal buffer and returns non-zero error.
int wrc_encoder_enc(WrcEncoder* e, CxVar* msg);

// Get last message encoded
void* wrc_encoder_msg(WrcEncoder* e, bool* text, size_t* len);

//-----------------------------------------------------------------------------
// Decoder
//-----------------------------------------------------------------------------

// Creates message decoder
typedef struct WrcDecoder WrcDecoder;
WrcDecoder* wrc_decoder_new(const CxAllocator* alloc);

// Destroy previously created decoder, deallocating memory
void wrc_decoder_del(WrcDecoder* d);

// Clear decoder state, without deallocating memory
void wrc_decoder_clear(WrcDecoder* e);

// Decodes message and returns non zero error code if message is invalid
int wrc_decoder_dec(WrcDecoder* d, bool text, void* data, size_t len, CxVar* msg);


#endif

