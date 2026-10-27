#ifndef BIOS_H
#define BIOS_H

#include <string>
#include "Memory.h"
#include "Archive.h"

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int uint;

extern bool bios_Load(std::string filename);
extern bool bios_IsLoaded();
extern void bios_Store();
extern void bios_Release();
extern std::string bios_filename;
extern bool bios_enabled;

#endif
