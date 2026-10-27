#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#define CARTRIDGE_TYPE_NORMAL 0
#define CARTRIDGE_TYPE_SUPERCART 1
#define CARTRIDGE_TYPE_SUPERCART_LARGE 2
#define CARTRIDGE_TYPE_SUPERCART_RAM 3
#define CARTRIDGE_TYPE_SUPERCART_ROM 4
#define CARTRIDGE_TYPE_ABSOLUTE 5
#define CARTRIDGE_TYPE_ACTIVISION 6
#define CARTRIDGE_CONTROLLER_NONE 0
#define CARTRIDGE_CONTROLLER_JOYSTICK 1
#define CARTRIDGE_CONTROLLER_LIGHTGUN 2
#define CARTRIDGE_WSYNC_MASK 2
#define CARTRIDGE_CYCLE_STEALING_MASK 1

#ifndef NULL
#define NULL 0
#endif

#include <cstdio>
#include <string>
#include "Equates.h"
#include "Memory.h"
#include "Hash.h"
#include "Pokey.h"
#include "Archive.h"

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int uint;

extern bool cartridge_Load(std::string filename);
extern void cartridge_Store();
extern void cartridge_StoreBank(byte bank);
extern void cartridge_Write(word address, byte data);
extern bool cartridge_IsLoaded();
extern void cartridge_Release();
extern std::string cartridge_digest;
extern std::string cartridge_title;
extern std::string cartridge_description;
extern std::string cartridge_year;
extern std::string cartridge_maker;
extern std::string cartridge_filename;
extern byte cartridge_type;
extern byte cartridge_region;
extern bool cartridge_pokey;
extern byte cartridge_controller[2];
extern byte cartridge_bank;
extern uint cartridge_flags;

// Extra per-cartridge settings from ProSystem.dat
extern int cartridge_crossx;
extern int cartridge_crossy;
extern bool cartridge_hsc;
extern bool cartridge_xm;
extern bool cartridge_pokey450;
extern byte cartridge_hblank;
extern bool cartridge_dualanalog;
extern bool cartridge_leftswitch;
extern bool cartridge_rightswitch;
extern bool cartridge_swapbuttons;
extern void cartridge_ResetExtras();

#endif // CARTRIDGE_H
