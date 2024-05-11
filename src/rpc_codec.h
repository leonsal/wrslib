#ifndef RPC_CODEC_H
#define RPC_CODEC_H

#include <stdbool.h>
#include <time.h>

#include "cx_error.h"
#include "cx_alloc.h"
#include "cx_var.h"

// Encoded binary message chunk types
typedef enum {
    WrsChunkMsg = 1,
    WrsChunkBuf,
    WrsChunkTypeInvalid,
} WrsChunkType;

// Creates message encoder using specified allocator.
typedef struct WrsEncoder WrsEncoder;
WrsEncoder* wrs_encoder_new(const CxAllocator* alloc);

// Destroy previously created message encoder, deallocating memory
void wrs_encoder_del(WrsEncoder*);

// Clear message encoder internal buffers, without deallocating memory
void wrs_encoder_clear(WrsEncoder* e);

// Encodes message into internal buffer
CxError wrs_encoder_enc(WrsEncoder* e, CxVar* msg);

// Get the type, pointer and length of last message encoded buffer.
void* wrs_encoder_get_msg(WrsEncoder* e, bool* text, size_t* len);

//-----------------------------------------------------------------------------
// Decoder
//-----------------------------------------------------------------------------

// Creates message decoder using specified allocator
typedef struct WrsDecoder WrsDecoder;
WrsDecoder* wrs_decoder_new(const CxAllocator* alloc);

// Destroy previously created message decoder, deallocating memory
void wrs_decoder_del(WrsDecoder* d);

// Clear message decoder state, without deallocating memory
void wrs_decoder_clear(WrsDecoder* e);

// Decodes message text or binary message
CxError wrs_decoder_dec(WrsDecoder* d, bool text, void* data, size_t len, CxVar* msg);


#endif

