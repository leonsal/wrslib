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

// Describe a binary buffer to encode/decoded
typedef struct BufInfo {
    void*   data;
    size_t  len;
} BufInfo;

// Define internal array of encoded buffers info
#define cx_array_name cxarr_buf
#define cx_array_type BufInfo
#define cx_array_implement
#define cx_array_instance_allocator
#define cx_array_static
#include "cx_array.h"

// Define internal array of CxVar*
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
    //cxarr_var   vars;       // Replaced vars
} WrsEncoder;


#define BUFFER_PREFIX   "\b\b\b\b\b\b"
#define CHUNK_ALIGNMENT sizeof(uint32_t)

static int enc_writer(void* ctx, const void* data, size_t len);
static void enc_json_replacer(CxVar* val, void* userdata);
static uintptr_t align_forward(uintptr_t ptr, size_t align);
static void add_padding(WrsEncoder*e, size_t align);
static void dec_json_replacer(CxVar* val, void* userdata);

#include "rpc_codec.h"

WrsEncoder* wrs_encoder_new(const CxAllocator* alloc) {

    WrsEncoder* e = cx_alloc_malloc(alloc, sizeof(WrsEncoder));
    e->alloc = alloc;
    e->encoded = cxarr_u8_init(alloc); 
    e->buffers = cxarr_buf_init(alloc);
    //e->vars = cxarr_var_init(alloc);
    return e;
}

void wrs_encoder_del(WrsEncoder* e) {

    cxarr_u8_free(&e->encoded);
    cxarr_buf_free(&e->buffers);
    //cxarr_var_free(&e->vars);
    cx_alloc_free(e->alloc, e, sizeof(WrsEncoder));
}

void wrs_encoder_clear(WrsEncoder* e) {

    cxarr_u8_clear(&e->encoded);
    cxarr_buf_clear(&e->buffers);
    //cxarr_var_clear(&e->vars);
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
        BufInfo* buf = &e->buffers.data[i];
        header = (ChunkHeader){.type = WrsChunkBuf, .size = buf->len };
        cxarr_u8_pushn(&e->encoded, (uint8_t*)&header, sizeof(ChunkHeader));

        // Writes the chunk data and padding
        cxarr_u8_pushn(&e->encoded, (uint8_t*)buf->data, buf->len);
        add_padding(e, CHUNK_ALIGNMENT);
    }

    // Free buffers
    for (size_t i = 0; i < cxarr_buf_len(&e->buffers); i++) {
        BufInfo* buf = &e->buffers.data[i];
        cx_alloc_free(e->alloc, buf->data, buf->len);
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
    cxarr_buf   buffers;        // Array of decoded buffers
    cxarr_var   vars;           // Array of CxVar buffers
} WrsDecoder;


WrsDecoder* wrs_decoder_new(const CxAllocator* alloc) {

    WrsDecoder* d = cx_alloc_malloc(alloc, sizeof(WrsDecoder));
    d->alloc = alloc;
    d->buffers = cxarr_buf_init(alloc);
    d->vars = cxarr_var_init(alloc);
    return d;
}

void wrs_decoder_del(WrsDecoder* d) {

    cxarr_buf_free(&d->buffers);
    cxarr_var_free(&d->vars);
    cx_alloc_free(d->alloc, d, sizeof(WrsDecoder));
}

void wrs_decoder_clear(WrsDecoder* d) {

    cxarr_buf_clear(&d->buffers);
    cxarr_var_clear(&d->vars);
}

int wrs_decoder_dec(WrsDecoder* d, bool text, void* data, size_t len, CxVar* msg) {

    // Sets the configuration for JSON parser
    CxJsonParseCfg cfg = {
        .alloc = d->alloc,
        .replacer_fn = dec_json_replacer,
        .replacer_data = d,
    };

    // Text messages only contains a JSON string
    if (text) {
        return cx_json_parse(data, len, msg, &cfg);
    }

    // Decode the message chunks in any order
    void* last = data + len;
    void* curr = data;
    bool json = false;
    while (curr < last) {
        // Checks available size for chunk header
        if (curr + sizeof(uint32_t)*2 > last) {
            return 1;
        }

        // Get the chunk type and length in bytes
        uint32_t chunk_type = *(uint32_t*)curr;
        curr += sizeof(uint32_t);
        uint32_t chunk_len = *(uint32_t*)curr;
        curr += sizeof(uint32_t);

        // Checks available size for chunk data
        if (curr + chunk_len > last) {
            return 1;
        }

        // Checks for JSON chunk
        if (chunk_type == WrsChunkMsg) {
            // Checks if JSON already decoded.
            // Only one JSON chunk is allowed.
            if (json) {
                return 1;
            }
            int res =cx_json_parse(curr, chunk_len, msg, &cfg);
            if (res) {
                return res;
            }
        // Checks for Buffer chunk
        } else if (chunk_type == WrsChunkBuf) {
            cxarr_buf_push(&d->buffers, (BufInfo){
                 .len = chunk_len,
                 .data = curr,
            });
        // Invalid chunk type
        } else {
            return 1;
        }

        // Advance pointer to start of next possible chunk
        curr += chunk_len;
        curr = (void*)align_forward((uintptr_t)curr, 4);
    }
   
    // Checks for exact length of binary message
    if (curr != last) {
        return 1;
    }

    // Converts decoded string CxVars to corresponding buffers
    if (cxarr_var_len(&d->vars) != cxarr_buf_len(&d->buffers)) {
        return 1;
    }
    for (size_t i = 0; i < cxarr_var_len(&d->vars); i++) {
        BufInfo* buf = &d->buffers.data[i];
        cx_var_set_buf(d->vars.data[i], (void*)buf->data, buf->len);
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Local functions
//-----------------------------------------------------------------------------


static int enc_writer(void* ctx, const void* data, size_t len) {

    WrsEncoder* e = ctx;
    cxarr_u8_pushn(&e->encoded, (char*)data, len);
    return len;
}

static void enc_json_replacer(CxVar* var, void* userdata) {

    if (cx_var_get_type(var) != CxVarBuf) {
        return;
    }

    // Get buffer data and len, makes a copy and saves into internal array
    WrsEncoder* e = userdata;
    const void* data;
    size_t len;
    cx_var_get_buf(var, &data, &len);
    BufInfo buffer = {
        .data = cx_alloc_malloc(e->alloc, len),
        .len = len,
    };
    memcpy(buffer.data, data, len);
    cxarr_buf_push(&e->buffers, buffer);

    // Replaces CxVar buffer with string with special prefix and buffer number
    // This will deallocate its CxVar buffer.
    int64_t nbufs = cxarr_buf_len(&e->buffers);
    char fmtbuf[32];
    snprintf(fmtbuf, sizeof(fmtbuf), BUFFER_PREFIX"%ld", nbufs-1);
    cx_var_set_str(var, fmtbuf);
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

static void dec_json_replacer(CxVar* var, void* userdata) {

    if (cx_var_get_type(var) != CxVarStr) {
        return;
    }
    const char* str;
    cx_var_get_str(var, &str);
    if (strncmp(str, BUFFER_PREFIX, strlen(BUFFER_PREFIX)) != 0) {
        return;
    }

    WrsDecoder* d = userdata;
    ssize_t nbuf = strtol(str + strlen(BUFFER_PREFIX), NULL, 10);
    printf("buffer:%ld\n", nbuf);
    cxarr_var_push(&d->vars, var);
}


