#include "rpc_codec.h"

WrsEncoder* wrs_encoder_new(const CxAllocator* alloc) {

    return NULL;
}

void wrs_encoder_del(WrsEncoder*) {

}


void wrs_encoder_clear(WrsEncoder* e) {

}

int wrs_encoder_enc(WrsEncoder* e, CxVar* msg) {

    return 0;
}

void* wrs_encoder_msg(WrsEncoder* e, bool* text, size_t* len) {

    return NULL;
}

//-----------------------------------------------------------------------------
// Decoder
//-----------------------------------------------------------------------------

// Creates message decoder
WrsDecoder* wrs_decoder_new(const CxAllocator* alloc) {

    return NULL;
}

void wrs_decoder_del(WrsDecoder* d) {

}

void wrs_decoder_clear(WrsDecoder* e) {


}


int wrs_decoder_dec(WrsDecoder* d, bool text, void* data, size_t len, CxVar* msg) {

    return 0;
}

