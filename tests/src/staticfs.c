//
// Embeds static filesystem in this translation unit
// The directory of the zip file is relative to CMake build directory
//
#include "incbin.h"
INCBIN(StaticfsZip, "../src/staticfs.zip");
extern const unsigned char gStaticfsZipData[];
extern const unsigned int  gStaticfsZipSize;

