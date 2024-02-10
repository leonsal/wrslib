//
// Embeds static filesystem in the executable
//
#include "incbin.h"
INCBIN(StaticfsZip, "../staticfs.zip");
extern const unsigned char gStaticfsZipData[];
extern const unsigned int  gStaticfsZipSize;

