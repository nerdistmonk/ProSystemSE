#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include "Cartridge.h"
#include "Common.h"

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int uint;

extern void database_Initialize();
extern bool database_Load(std::string digest);
extern bool database_enabled;
extern std::string database_filename;

#endif
