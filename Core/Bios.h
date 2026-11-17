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
extern bool bios_bypass;
extern bool bios_waiting;
extern bool bios_Restarted(word pc);
extern void bios_CheckHandoff(word pc);

#endif
