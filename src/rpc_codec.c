/*
    RPC binary message encoder/decoder

    Server sends to browser message in CxVar: 
        { n: 1, bufa:<CxVarBuf>, bufb:<CxVarBuf>}

        Is encoded as JSON as:
        { n: 1, bufa:"CxVarBuf:0:", bufb:"CxVarBuf:1"}

        Binary message sent is:
        - JSON chunk
        - Buffer a chunk
        - Buffer b chunk

        Browser decodes message as:
        { n: 1, bufa:<array buffer>, bufb:<array buffer>}

    Browser sends to server:
        { n: 1, bufa:<typed array>, bufb:<typed array>}

        Is encoded as JSON as:
        { n: 1, bufa: "Buf0", bufb: "Buf1"}

        Binary message sent is:
        - JSON chunk
        - Buffer a chunk
        - Buffer b chunk

        Server decodes message as a CxVar with the fields:
        { n: 1, bufa:<CxVarBuf>, bufb:<CxVarBuf>}

    Each chunk consists of a header, data and padding
        - Chunk type (uint32_t)
        - Chunk size (uint32_t)
        - Chunk data
        - padding to align to multiple of 4

*/
#include <stddef.h>
#include <stdio.h>

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
} WrsEncoder;

#include "cx_var.h"
#include "cx_json_parse.h"

#define CHUNK_ALIGNMENT sizeof(uint32_t)

static int enc_json(WrsEncoder* e, CxVar* msg);
static int enc_json_null(WrsEncoder* e, const CxVar* var);
static int enc_json_bool(WrsEncoder* e, const CxVar* var);
static int enc_json_int(WrsEncoder* e, const CxVar* var);
static int enc_json_float(WrsEncoder* e, const CxVar* var);
static int enc_json_str(WrsEncoder* e, const CxVar* var);
static int enc_json_arr(WrsEncoder* e, const CxVar* var);
static int enc_json_map(WrsEncoder* e, const CxVar* var);
static int enc_json_buf(WrsEncoder* e, const CxVar* var);
static uintptr_t align_forward(uintptr_t ptr, size_t align);
static void add_padding(WrsEncoder*e, size_t align);

#include "rpc_codec.h"

WrsEncoder* wrs_encoder_new(const CxAllocator* alloc) {

    WrsEncoder* e = cx_alloc_malloc(alloc, sizeof(WrsEncoder));
    e->alloc = alloc;
    e->encoded = cxarr_u8_init(alloc); 
    e->buffers = cxarr_buf_init(alloc);
    return e;
}

void wrs_encoder_del(WrsEncoder* e) {

    cxarr_u8_free(&e->encoded);
    cxarr_buf_free(&e->buffers);
    cx_alloc_free(e->alloc, e, sizeof(WrsEncoder));
}


void wrs_encoder_clear(WrsEncoder* e) {

    cxarr_u8_clear(&e->encoded);
    cxarr_buf_clear(&e->buffers);
}

int wrs_encoder_enc(WrsEncoder* e, CxVar* msg) {

    // Clear the internal buffers
    cxarr_u8_clear(&e->encoded);
    cxarr_buf_clear(&e->buffers);

    // Write JSON chunk header
    ChunkHeader header = {.type = WrsChunkMsg };
    cxarr_u8_pushn(&e->encoded, (uint8_t*)&header, sizeof(ChunkHeader));

    // Encodes JSON
    int res = enc_json(e, msg);
    if (res) {
        return res;
    }

    // Sets the msg size in the first chunk
    uint32_t msg_size = cxarr_u8_len(&e->encoded) - sizeof(ChunkHeader);
    ((ChunkHeader*)e->encoded.data)->size = msg_size;

    // Appends binary buffers to encoded data
    for (size_t i = 0; i < cxarr_buf_len(&e->buffers); i++) {


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

int enc_json(WrsEncoder* e, CxVar* var) {

    int res;
    switch(cx_var_get_type(var)) {
        case CxVarNull:
            res = enc_json_null(e, var);
            break;
        case CxVarBool:
            res = enc_json_bool(e, var);
            break;
        case CxVarInt:
            res = enc_json_int(e, var);
            break;
        case CxVarFloat:
            res = enc_json_float(e, var);
            break;
        case CxVarStr:
            res = enc_json_str(e, var);
            break;
        case CxVarArr:
            res = enc_json_arr(e, var);
            break;
        case CxVarMap:
            res = enc_json_map(e, var);
            break;
        case CxVarBuf:
            res = enc_json_buf(e, var);
            break;
        default:
            return 1;
    }
    if (res < 0) {
        return 1;
    }
    return 0;
}

static int enc_json_null(WrsEncoder* e, const CxVar* var) {

    cxarr_u8_pushn(&e->encoded, (uint8_t*)"null", 4);
    return 0;
}

static int enc_json_bool(WrsEncoder* e, const CxVar* var) {

    bool val;
    cx_var_get_bool(var, &val);
    if (val) {
        cxarr_u8_pushn(&e->encoded, (uint8_t*)"true", 4);
    } else {
        cxarr_u8_pushn(&e->encoded, (uint8_t*)"false", 5);
    }
    return 0;
}

static int enc_json_int(WrsEncoder* e, const CxVar* var) {

    int64_t val;
    cx_var_get_int(var, &val);
    char fmtbuf[256];
    sprintf(fmtbuf, "%zd", val);
    cxarr_u8_pushn(&e->encoded, (uint8_t*)fmtbuf, strlen(fmtbuf));
    return 0;
}

static int enc_json_float(WrsEncoder* e, const CxVar* var) {

    double val;
    cx_var_get_float(var, &val);
    char fmtbuf[256];
    sprintf(fmtbuf, "%f", val);
    cxarr_u8_pushn(&e->encoded, (uint8_t*)fmtbuf, strlen(fmtbuf));
    return 0;
}

static int enc_json_str(WrsEncoder* e, const CxVar* var) {

    const char* str;
    cx_var_get_str(var, &str);

    size_t len = strlen(str);
    cxarr_u8_push(&e->encoded, '\"');
    for (size_t i = 0; i < len; i++) {
        int c = str[i];
        switch (c) {
            case '"':
                cxarr_u8_pushn(&e->encoded, (uint8_t*)"\\\"", 2);
                break;
            case '\\':
                cxarr_u8_pushn(&e->encoded, (uint8_t*)"\\\\", 2);
                break;
            case '\b':
                cxarr_u8_pushn(&e->encoded, (uint8_t*)"\\b", 2);
                break;
            case '\f':
                cxarr_u8_pushn(&e->encoded, (uint8_t*)"\\f", 2);
                break;
            case '\n':
                cxarr_u8_pushn(&e->encoded, (uint8_t*)"\\n", 2);
                break;
            case '\r':
                cxarr_u8_pushn(&e->encoded, (uint8_t*)"\\r", 2);
                break;
            case '\t':
                cxarr_u8_pushn(&e->encoded, (uint8_t*)"\\t", 2);
                break;
            default:
                cxarr_u8_push(&e->encoded, c);
                break;
        }
    }
    cxarr_u8_push(&e->encoded, '\"');
    return 0;
}

static int enc_json_arr(WrsEncoder* e, const CxVar* var) {

    size_t len;
    cx_var_get_arr_len(var, &len);
    cxarr_u8_push(&e->encoded, '[');
    for (size_t i = 0; i < len; i++) {
        CxVar* el = cx_var_get_arr_val(var, i);
        enc_json(e, el);
        if (i < len - 1) {
            cxarr_u8_push(&e->encoded, ',');
        }
    }
    cxarr_u8_push(&e->encoded, ']');
    return 0;
}

static int enc_json_map(WrsEncoder* e, const CxVar* var) {

    size_t count;
    cx_var_get_map_len(var, &count);
    size_t order = 0;
    cxarr_u8_push(&e->encoded, '{');
    while (true) {
        const char* key = cx_var_get_map_key(var, order);
        if (key == NULL) {
             break;
        }
        CxVar* value = cx_var_get_map_val(var, key);
        if (value == NULL) {
            break;
        }
        cxarr_u8_push(&e->encoded, '\"');
        cxarr_u8_pushn(&e->encoded, (uint8_t*)key, strlen(key));
        cxarr_u8_pushn(&e->encoded, (uint8_t*)"\":", 2);
        enc_json(e, value);
        if (order < count - 1) {
            cxarr_u8_push(&e->encoded, ',');
        }
        order++;
    }
    cxarr_u8_push(&e->encoded, '}');
    return 0;
}

static int enc_json_buf(WrsEncoder* e, const CxVar* var) {

    // Get buffer data and len and saves into internal array
    EncBuffer buffer;
    cx_var_get_buf(var, &buffer.data, &buffer.len);
    cxarr_buf_push(&e->buffers, buffer);

    // Encodes as the number of the buffer
    size_t nbufs = cxarr_buf_len(&e->buffers);
    char fmtbuf[256];
    sprintf(fmtbuf, "%zu", nbufs);
    cxarr_u8_pushn(&e->encoded, (uint8_t*)fmtbuf, strlen(fmtbuf));
    return 0;
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


