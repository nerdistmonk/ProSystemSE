#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <string>
#include "Cartridge.h"

extern "C" {
#include "zip.h"
#include "unzip.h"
}

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int uint;

extern uint archive_GetUncompressedFileSize(std::string filename);
extern bool archive_Uncompress(std::string filename, byte* data, uint size);
extern bool archive_Compress(std::string zipFilename, std::string filename, const byte* data, uint size);

#endif
