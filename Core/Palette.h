#ifndef PALETTE_H
#define PALETTE_H

#define PALETTE_SIZE 768

#ifndef NULL
#define NULL 0
#endif

#include <string>

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int uint;

extern bool palette_Load(std::string filename);
extern void palette_Load(const byte* data);
extern std::string palette_filename;
extern byte palette_data[PALETTE_SIZE];
extern bool palette_default;

#endif // PALETTE_H
