/*
    RPC binary message encoder/decoder

    Server sends to browser message in CxVar: 
        { n: 1, bufa:<CxVarBuf>, bufb:<CxVarBuf>}

        Is encoded as JSON as:
        { n: 1, bufa:"WrsBuf:0", bufb:"WrsBuf:1"}

        Binary message sent is:
        - JSON chunk
        - Buffer a chunk
        - Buffer b chunk

        Browser decodes message as:
        { n: 1, bufa:<array buffer>, bufb:<array buffer>}

    Browser app sends to server:
        { n: 1, bufa:<typed array>, bufb:<typed array>}

        Is encoded by browser as JSON as:
        { n: 1, bufa: "WrsBuf:0", bufb: "WrsBuf:"}

        Binary message sent is:
        - JSON chunk
        - Buffer a chunk
        - Buffer b chunk

        Server decodes message as a CxVar with the fields:
        { n: 1, bufa:<CxVarBuf>, bufb:<CxVarBuf>}

    Each chunk consists of a header, data and padding
        - Chunk 0:
            - type (uint32_t)
            - size (uint32_t)
            - data
            - padding to align to multiple of 4
        ...
        - Chunk N:
            - type (uint32_t)
            - size (uint32_t)
            - data
            - padding to align to multiple of 4

*/
#include <stddef.h>
#include <stdio.h>

#include "cx_var.h"
#include "cx_json_build.h"
#include "cx_json_parse.h"

// Define internal array for encoded binary data
#define cx_array_name cxarr_u8
#define cx_array_type uint8_t
#define cx_array_implement
#define cx_array_instance_allocator
#define cx_array_static
#include "cx_array.h"

// Describe a binary buffer to encode
typedef struct EncBuffer {
    const void* data;
    size_t      len;
} EncBuffer;

// Define internal array of encoded buffers info
#define cx_array_name cxarr_buf
#define cx_array_type EncBuffer
#define cx_array_implement
#define cx_array_instance_allocator
#define cx_array_static
#include "cx_array.h"

// Define internal array of replaced CxVar for
// deallocation
#define cx_array_name cxarr_var
#define cx_array_type CxVar*
#define cx_array_implement
#define cx_array_instance_allocator
#define cx_array_static
#include "cx_array.h"

// Binary message chunk header
typedef struct ChunkHeader {
    uint32_t type;
    uint32_t size;
} ChunkHeader;

// Encoder state
typedef struct WrsEncoder {
    const CxAllocator* alloc;
    cxarr_u8    encoded;    // Buffer with encoded message chunks
    cxarr_buf   buffers;    // Array of buffers to encode
    cxarr_var   vars;       // Replaced vars
} WrsEncoder;


#define BUFFER_PREFIX   "\b\b\b\b\b\b"
#define CHUNK_ALIGNMENT sizeof(uint32_t)

static int enc_writer(void* ctx, const void* data, size_t len);
static CxVar* enc_json_replacer(CxVar* val, void* userdata);
static uintptr_t align_forward(uintptr_t ptr, size_t align);
static void add_padding(WrsEncoder*e, size_t align);

#include "rpc_codec.h"

WrsEncoder* wrs_encoder_new(const CxAllocator* alloc) {

    WrsEncoder* e = cx_alloc_malloc(alloc, sizeof(WrsEncoder));
    e->alloc = alloc;
    e->encoded = cxarr_u8_init(alloc); 
    e->buffers = cxarr_buf_init(alloc);
    e->vars = cxarr_var_init(alloc);
    return e;
}

void wrs_encoder_del(WrsEncoder* e) {

    cxarr_u8_free(&e->encoded);
    cxarr_buf_free(&e->buffers);
    cxarr_var_free(&e->vars);
    cx_alloc_free(e->alloc, e, sizeof(WrsEncoder));
}


void wrs_encoder_clear(WrsEncoder* e) {

    cxarr_u8_clear(&e->encoded);
    cxarr_buf_clear(&e->buffers);
    cxarr_var_clear(&e->vars);
}

int wrs_encoder_enc(WrsEncoder* e, CxVar* msg) {

    // Clear the internal buffers
    cxarr_u8_clear(&e->encoded);
    cxarr_buf_clear(&e->buffers);

    // Write JSON chunk header
    ChunkHeader header = {.type = WrsChunkMsg };
    cxarr_u8_pushn(&e->encoded, (uint8_t*)&header, sizeof(ChunkHeader));

    // Encodes JSON
    CxJsonBuildCfg cfg = { .replacer_fn = enc_json_replacer, .replacer_data = e };
    CxWriter writer = {.ctx = e, .write = (CxWriterWrite)enc_writer};
    int res = cx_json_build(msg, &cfg, &writer); 
    if (res) {
        return res;
    }

    // Sets the msg size in the first chunk and adds padding
    uint32_t msg_size = cxarr_u8_len(&e->encoded) - sizeof(ChunkHeader);
    ((ChunkHeader*)e->encoded.data)->size = msg_size;
    add_padding(e, CHUNK_ALIGNMENT);

    // Appends binary buffers to encoded data
    for (size_t i = 0; i < cxarr_buf_len(&e->buffers); i++) {

        // Writes the chunk header
        EncBuffer* buf = &e->buffers.data[i];
        header = (ChunkHeader){.type = WrsChunkBuf, .size = buf->len };
        cxarr_u8_pushn(&e->encoded, (uint8_t*)&header, sizeof(ChunkHeader));

        // Writes the chunk data and padding
        cxarr_u8_pushn(&e->encoded, (uint8_t*)buf->data, buf->len);
        add_padding(e, CHUNK_ALIGNMENT);
    }

    return 0;
}

void* wrs_encoder_get_msg(WrsEncoder* e, bool* text, size_t* len) {

    // Checks if encoded buffer is empty
    const size_t buf_size = cxarr_u8_len(&e->encoded);
    if (buf_size == 0) {
        *len = 0;
        return NULL;
    }

    // Checks for text message (no binary buffers present)
    if (cxarr_buf_len(&e->buffers) == 0) {
        *text = true;
        *len = buf_size - sizeof(ChunkHeader);
        return e->encoded.data + sizeof(ChunkHeader);
    } else {
        *text = false;
        *len = buf_size;
        return e->encoded.data;
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// Decoder
//-----------------------------------------------------------------------------

// Decoder state
typedef struct WrsDecoder {
    const CxAllocator* alloc;   // Custom allocator
} WrsDecoder;


WrsDecoder* wrs_decoder_new(const CxAllocator* alloc) {

    WrsDecoder* d = cx_alloc_malloc(alloc, sizeof(WrsDecoder));
    d->alloc = alloc;
    return d;
}

void wrs_decoder_del(WrsDecoder* d) {

    cx_alloc_free(d->alloc, d, sizeof(WrsDecoder));
}

void wrs_decoder_clear(WrsDecoder* e) {

    //arr_buf_clear(&d->buffers);

}


int wrs_decoder_dec(WrsDecoder* d, bool text, void* data, size_t len, CxVar* msg) {

    // Text messages only contains a JSON string
    if (text) {
        return cx_json_parse(data, len, msg, d->alloc);
    }

    // Decode the message chunks in any order
    void* last = data + len;
    void* p = data;
    bool json = false;
    while (1) {
        // Checks available size for chunk header
        if (p + sizeof(uint32_t)*2 > last) {
            return 1;
        }

        // Get the chunk type and length in bytes
        uint32_t chunk_type = *(uint32_t*)p;
        p += sizeof(uint32_t);
        uint32_t chunk_len = *(uint32_t*)p;
        p += sizeof(uint32_t);

        // Checks available size for chunk data
        if (p + chunk_len > last) {
            return 1;
        }

        // Checks for JSON chunk
        if (chunk_type == WrsChunkMsg) {
            // Checks if JSON already decoded.
            // Only one JSON chunk is allowed.
            if (json) {
                return 1;
            }
            int res =cx_json_parse(p, chunk_len, msg, d->alloc);
            if (res) {
                return res;
            }
        // Checks for Buffer chunk
        } else if (chunk_type == WrsChunkBuf) {
            // arr_buf_push(&d->buffers, (BufInfo){
            //     .type = 1,
            //     .len = chunk_len,
            //     .data = p,
            // });
        } else {
            return 1;
        }

        // Advance pointer to start of next possible chunk
        p += chunk_len;
        p = (void*)align_forward((uintptr_t)p, 4);
        // Checks for more chunks
        if (p == last) {
            return 0;
        }
    }
    return 1;
}

//-----------------------------------------------------------------------------
// Local functions
//-----------------------------------------------------------------------------


static int enc_writer(void* ctx, const void* data, size_t len) {

    WrsEncoder* e = ctx;
    cxarr_u8_pushn(&e->encoded, (char*)data, len);
    return len;
}

static CxVar* enc_json_replacer(CxVar* var, void* userdata) {

    WrsEncoder* e = userdata;
    if (cx_var_get_type(var) != CxVarBuf) {
        return var;
    }

    // Get buffer data and len and saves into internal array
    EncBuffer buffer;
    cx_var_get_buf(var, &buffer.data, &buffer.len);
    cxarr_buf_push(&e->buffers, buffer);

    // Replaces buffer with string with special prefix and buffer number
    int64_t nbufs = cxarr_buf_len(&e->buffers);
    char fmtbuf[32];
    snprintf(fmtbuf, sizeof(fmtbuf), BUFFER_PREFIX"%ld", nbufs-1);
    CxVar* replaced = cx_var_new(cxDefaultAllocator());
    cx_var_set_str(replaced, fmtbuf);
    cxarr_var_push(&e->vars, replaced);
    return replaced;
}


// Returns the aligned pointer for the specified pointer and desired alignment
static uintptr_t align_forward(uintptr_t ptr, size_t align) {

	uintptr_t p = ptr;
	uintptr_t a = (uintptr_t)align;
	uintptr_t modulo = p & (a-1);
	if (modulo != 0) {
		p += a - modulo;
	}
	return p;
}

static void add_padding(WrsEncoder*e, size_t align) {

    const uintptr_t buf_len = cxarr_u8_len(&e->encoded);
	uintptr_t a = (uintptr_t)align;
    uintptr_t new_len = buf_len;
	uintptr_t modulo = new_len & (a-1);
	if (modulo != 0) {
		new_len += a - modulo;
	}
    const size_t npaddings = new_len - buf_len;
    static char* padbytes[8] = {0};
    cxarr_u8_pushn(&e->encoded, (char*)padbytes, npaddings);
}


