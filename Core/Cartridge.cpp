// ----------------------------------------------------------------------------
//   ___  ___  ___  ___       ___  ____  ___  _  _
//  /__/ /__/ /  / /__  /__/ /__    /   /_   / |/ /
// /    / \  /__/ ___/ ___/ ___/   /   /__  /    /  emulator
//
// ----------------------------------------------------------------------------
// Original Copyright 2005 Greg Stanton
// SDL3 Linux port Copyright 2026 R.Shaklfrd
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// ----------------------------------------------------------------------------
// Cartridge.cpp
// ----------------------------------------------------------------------------
#include "Cartridge.h"
#include "Memory.h"
#include "Hash.h"
#include "Archive.h"
#include "Bios.h"
#include "Common.h"

std::string cartridge_title;
std::string cartridge_description;
std::string cartridge_year;
std::string cartridge_maker;
std::string cartridge_digest;
std::string cartridge_filename;
byte cartridge_type;
byte cartridge_region;
bool cartridge_pokey;
byte cartridge_controller[2];
byte cartridge_bank;
uint cartridge_flags;
int cartridge_crossx = 0;
int cartridge_crossy = 0;
bool cartridge_hsc = false;
bool cartridge_xm = false;
byte cartridge_souper_chr_bank[2] = {0, 0};
byte cartridge_souper_mode = 0;
byte cartridge_souper_ram_page_bank[2] = {0, 0};
bool cartridge_pokey450 = false;
bool cartridge_pokey440 = false;
bool cartridge_pokey800 = false;
// Where the POKEY sits when it is mapped into RAM space (0 = none); $4000 is handled by cartridge_pokey.
word cartridge_pokey_base = 0;
byte cartridge_hblank = 0;
bool cartridge_dualanalog = false;
bool cartridge_leftswitch = false;
bool cartridge_rightswitch = false;
bool cartridge_swapbuttons = false;

void cartridge_ResetExtras() {
    cartridge_crossx = 0;
    cartridge_crossy = 0;
    cartridge_hsc = false;
    cartridge_xm = false;
    cartridge_pokey450 = false;
    cartridge_pokey440 = false;
    cartridge_pokey800 = false;
    cartridge_pokey_base = 0;
    cartridge_hblank = 0;
    cartridge_dualanalog = false;
    cartridge_leftswitch = false;
    cartridge_rightswitch = false;
    cartridge_swapbuttons = false;
}

static byte* cartridge_buffer = NULL;
static uint cartridge_size = 0;
// Largest ROM window any mapper reads (Souper: 32 banks of 16K)
static const uint CARTRIDGE_MAX_MAPPED_SIZE = 32 * 16384;

// ----------------------------------------------------------------------------
// HasHeader
// ----------------------------------------------------------------------------
static bool cartridge_HasHeader(const byte* header) {
    const char HEADER_ID[] = {"ATARI7800"};
    for (int index = 0; index < 9; index++) {
        if (HEADER_ID[index] != header[index + 1]) {
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// Header for CC2 hack
// ----------------------------------------------------------------------------
static bool cartridge_CC2(const byte* header) {
    const char HEADER_ID[] = {">>"};
    for (int index = 0; index < 2; index++) {
        if (HEADER_ID[index] != header[index + 1]) {
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// GetBankOffset
// ----------------------------------------------------------------------------
static uint cartridge_GetBankOffset(byte bank) {
    // Supercart ROMs of 64K or less only have 4 banks; the bank select ignores bit 2.
    if ((cartridge_type == CARTRIDGE_TYPE_SUPERCART ||
         cartridge_type == CARTRIDGE_TYPE_SUPERCART_ROM ||
         cartridge_type == CARTRIDGE_TYPE_SUPERCART_RAM) && cartridge_size <= 65536) {
        return (bank & 3) * 16384;
    }
    return bank * 16384;
}

// ----------------------------------------------------------------------------
// WriteBank
// ----------------------------------------------------------------------------
static void cartridge_WriteBank(word address, byte bank) {
    uint offset = cartridge_GetBankOffset(bank);
    if (offset < cartridge_size) {
        memory_WriteROM(address, 16384, cartridge_buffer + offset);
        cartridge_bank = bank;
    }
}

// ----------------------------------------------------------------------------
// LoadROM (raw cartridge data, used by Souper CHR banking)
// ----------------------------------------------------------------------------
byte cartridge_LoadROM(uint address) {
    if (address >= cartridge_size) {
        return 0;
    }
    return cartridge_buffer[address];
}

// ----------------------------------------------------------------------------
// Souper
// ----------------------------------------------------------------------------
void cartridge_souper_Reset() {
    cartridge_souper_chr_bank[0] = 0;
    cartridge_souper_chr_bank[1] = 0;
    cartridge_souper_mode = 0;
    cartridge_souper_ram_page_bank[0] = 0;
    cartridge_souper_ram_page_bank[1] = 0;
}

// ----------------------------------------------------------------------------
// UpdatePokeyBase
// ----------------------------------------------------------------------------
void cartridge_UpdatePokeyBase() {
    if (cartridge_pokey800) {
        cartridge_pokey_base = 0x800;
    }
    else if (cartridge_pokey440) {
        cartridge_pokey_base = 0x440;
    }
    else if (cartridge_pokey450) {
        cartridge_pokey_base = 0x450;
    }
    else {
        cartridge_pokey_base = 0;
    }
}

// ----------------------------------------------------------------------------
// ReadHeader
// ----------------------------------------------------------------------------
static void cartridge_ReadHeader(const byte* header) {
    char temp[33] = {0};
    for (int index = 0; index < 32; index++) {
        temp[index] = header[index + 17];
    }
    cartridge_title = temp;

    cartridge_size = static_cast<uint>(header[49]) << 24;
    cartridge_size |= static_cast<uint>(header[50]) << 16;
    cartridge_size |= static_cast<uint>(header[51]) << 8;
    cartridge_size |= static_cast<uint>(header[52]);

    // Bit 6 of byte 54 is a POKEY at $0450, bit 2 of byte 53 one at $0440 and bit 7 one
    // at $0800; none of them affect the bankswitching scheme.
    cartridge_pokey450 = (header[54] & 0x40) ? true : false;
    cartridge_pokey440 = (header[53] & 0x04) ? true : false;
    cartridge_pokey800 = (header[53] & 0x80) ? true : false;
    byte typeByte = header[54] & ~0x40;
    // Only Activision (1) and Absolute (2) are supported in byte 53; other bits
    // (YM2151, Souper, banksets...) are ignored so the cart still boots.
    byte mapperByte = (header[53] == 1 || header[53] == 2 || header[53] == 0x10) ? header[53] : 0;
    // SuperGame carts have a power-of-two bank count, except the 9-bank (144K) layout
    // and its larger variants which have one extra 16K bank.
    bool oddBanks = ((cartridge_size / 16384) & 1) != 0;
    if (mapperByte == 0) {
        if (typeByte >= 2 && typeByte <= 7 && !oddBanks) {
            cartridge_type = (typeByte >= 4) ? CARTRIDGE_TYPE_SUPERCART_RAM : CARTRIDGE_TYPE_SUPERCART;
        }
        else if (cartridge_size > 131072) {
            cartridge_type = CARTRIDGE_TYPE_SUPERCART_LARGE;
        }
        else if ((typeByte & 0x10) && cartridge_size == 131072) {
            // Bit 4 maps bank 6 at $4000, which is the SuperGame ROM layout
            cartridge_type = CARTRIDGE_TYPE_SUPERCART_ROM;
        }
        else if (typeByte == 2 || typeByte == 3) {
            cartridge_type = CARTRIDGE_TYPE_SUPERCART;
        }
        else if (typeByte >= 4 && typeByte <= 7) {
            cartridge_type = CARTRIDGE_TYPE_SUPERCART_RAM;
        }
        else if (typeByte >= 8 && typeByte <= 11) {
            cartridge_type = CARTRIDGE_TYPE_SUPERCART_ROM;
        }
        else {
            cartridge_type = CARTRIDGE_TYPE_NORMAL;
        }
    }
    else {
        if (mapperByte == 1) {
            cartridge_type = CARTRIDGE_TYPE_ACTIVISION;
        }
        else if (mapperByte == 2) {
            cartridge_type = CARTRIDGE_TYPE_ABSOLUTE;
        }
        else if (mapperByte == 0x10) {
            cartridge_type = CARTRIDGE_TYPE_SOUPER;
        }
        else {
            cartridge_type = CARTRIDGE_TYPE_NORMAL;
        }
    }

    cartridge_pokey = (header[54] & 1) ? true : false;
    cartridge_UpdatePokeyBase();
    cartridge_controller[0] = header[55];
    cartridge_controller[1] = header[56];
    cartridge_region = header[57];
    cartridge_flags = 0;
}

// ----------------------------------------------------------------------------
// Store
// ----------------------------------------------------------------------------
void cartridge_Store() {
    uint lastBank = (cartridge_size >= 16384) ? (cartridge_size / 16384 - 1) : 0;

    switch (cartridge_type) {
    case CARTRIDGE_TYPE_NORMAL:
        memory_WriteROM(65536 - cartridge_size, cartridge_size, cartridge_buffer);
        break;

    case CARTRIDGE_TYPE_SUPERCART:
        if (cartridge_GetBankOffset(lastBank) < cartridge_size) {
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(lastBank));
            memory_WriteROM(32768, 16384, cartridge_buffer + cartridge_GetBankOffset(0));
            if (cartridge_size <= 65536 && cartridge_size > 32768) {
                memory_WriteROM(16384, 16384, cartridge_buffer + cartridge_GetBankOffset(0));
            }
            cartridge_bank = 0;
        }
        break;

    case CARTRIDGE_TYPE_SUPERCART_LARGE:
        if (cartridge_GetBankOffset(lastBank) < cartridge_size) {
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(lastBank));
            memory_WriteROM(32768, 16384, cartridge_buffer + cartridge_GetBankOffset(0));
            memory_WriteROM(16384, 16384, cartridge_buffer + cartridge_GetBankOffset(0));
            cartridge_bank = 0;
        }
        break;

    case CARTRIDGE_TYPE_SUPERCART_RAM:
        if (cartridge_GetBankOffset(lastBank) < cartridge_size) {
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(lastBank));
            memory_WriteROM(32768, 16384, cartridge_buffer + cartridge_GetBankOffset(0));
            memory_ClearROM(16384, 16384);
            cartridge_bank = 0;
        }
        break;

    case CARTRIDGE_TYPE_SUPERCART_ROM:
        if (cartridge_size >= 32768) {
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(lastBank));
            memory_WriteROM(32768, 16384, cartridge_buffer + cartridge_GetBankOffset(0));
            memory_WriteROM(16384, 16384, cartridge_buffer + cartridge_GetBankOffset(lastBank > 0 ? lastBank - 1 : 0));
            cartridge_bank = 0;
        }
        break;

    case CARTRIDGE_TYPE_ABSOLUTE:
        memory_WriteROM(16384, 16384, cartridge_buffer);
        memory_WriteROM(32768, 32768, cartridge_buffer + cartridge_GetBankOffset(2));
        break;

    case CARTRIDGE_TYPE_ACTIVISION:
        if (122880 < cartridge_size) {
            memory_WriteROM(40960, 16384, cartridge_buffer);
            memory_WriteROM(16384, 8192, cartridge_buffer + 106496);
            memory_WriteROM(24576, 8192, cartridge_buffer + 98304);
            memory_WriteROM(32768, 8192, cartridge_buffer + 122880);
            memory_WriteROM(57344, 8192, cartridge_buffer + 114688);
        }
        break;

    case CARTRIDGE_TYPE_SOUPER:
        if (cartridge_size >= 32 * 16384) {
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(31));
            memory_WriteROM(32768, 16384, cartridge_buffer + cartridge_GetBankOffset(0));
            memory_ClearROM(16384, 16384);
            cartridge_bank = 0;
        }
        break;
    }
}

// ----------------------------------------------------------------------------
// Write
// ----------------------------------------------------------------------------
void cartridge_Write(word address, byte data) {
    // Larger SuperGame carts (256K, 512K...) have more than the classic 9 selectable banks.
    uint bankLimit = cartridge_size / 16384;
    if (bankLimit < 9) {
        bankLimit = 9;
    }

    switch (cartridge_type) {
    case CARTRIDGE_TYPE_SUPERCART:
    case CARTRIDGE_TYPE_SUPERCART_RAM:
    case CARTRIDGE_TYPE_SUPERCART_ROM:
        if (address >= 32768 && address < 49152 && data < bankLimit) {
            cartridge_StoreBank(data);
        }
        break;
    case CARTRIDGE_TYPE_SUPERCART_LARGE:
        if (address >= 32768 && address < 49152 && data < bankLimit) {
            cartridge_StoreBank(data + 1);
        }
        break;
    case CARTRIDGE_TYPE_ABSOLUTE:
        if (address == 32768 && (data == 1 || data == 2)) {
            cartridge_StoreBank(data - 1);
        }
        break;
    case CARTRIDGE_TYPE_ACTIVISION:
        if (address >= 65408) {
            cartridge_StoreBank(address & 7);
        }
        break;
    case CARTRIDGE_TYPE_SOUPER:
        switch (address) {
        case CARTRIDGE_SOUPER_BANK_SEL:
            cartridge_StoreBank(data & 31);
            break;
        case CARTRIDGE_SOUPER_CHR_A_SEL:
            cartridge_souper_chr_bank[0] = data;
            break;
        case CARTRIDGE_SOUPER_CHR_B_SEL:
            cartridge_souper_chr_bank[1] = data;
            break;
        case CARTRIDGE_SOUPER_MODE_SEL:
            cartridge_souper_mode = data;
            break;
        case CARTRIDGE_SOUPER_EXRAM_V_SEL:
            cartridge_souper_ram_page_bank[0] = data & 7;
            break;
        case CARTRIDGE_SOUPER_EXRAM_D_SEL:
            cartridge_souper_ram_page_bank[1] = data & 7;
            break;
        }
        break;
    }

    if (cartridge_pokey_base && address >= cartridge_pokey_base && address < cartridge_pokey_base + 9) {
        pokey_SetRegister(address - cartridge_pokey_base + 0x4000, data);
    }

    if (cartridge_pokey && address >= 0x4000 && address < 0x4009) {
        switch (address) {
        case POKEY_AUDF1:  pokey_SetRegister(POKEY_AUDF1, data); break;
        case POKEY_AUDC1:  pokey_SetRegister(POKEY_AUDC1, data); break;
        case POKEY_AUDF2:  pokey_SetRegister(POKEY_AUDF2, data); break;
        case POKEY_AUDC2:  pokey_SetRegister(POKEY_AUDC2, data); break;
        case POKEY_AUDF3:  pokey_SetRegister(POKEY_AUDF3, data); break;
        case POKEY_AUDC3:  pokey_SetRegister(POKEY_AUDC3, data); break;
        case POKEY_AUDF4:  pokey_SetRegister(POKEY_AUDF4, data); break;
        case POKEY_AUDC4:  pokey_SetRegister(POKEY_AUDC4, data); break;
        case POKEY_AUDCTL: pokey_SetRegister(POKEY_AUDCTL, data); break;
        }
    }
}

// ----------------------------------------------------------------------------
// StoreBank
// ----------------------------------------------------------------------------
void cartridge_StoreBank(byte bank) {
    switch (cartridge_type) {
    case CARTRIDGE_TYPE_SUPERCART:
    case CARTRIDGE_TYPE_SUPERCART_RAM:
    case CARTRIDGE_TYPE_SUPERCART_ROM:
    case CARTRIDGE_TYPE_SUPERCART_LARGE:
    case CARTRIDGE_TYPE_SOUPER:
        cartridge_WriteBank(32768, bank);
        break;
    case CARTRIDGE_TYPE_ABSOLUTE:
        cartridge_WriteBank(16384, bank);
        break;
    case CARTRIDGE_TYPE_ACTIVISION:
        cartridge_WriteBank(40960, bank);
        break;
    }
}

// ----------------------------------------------------------------------------
// IsLoaded
// ----------------------------------------------------------------------------
bool cartridge_IsLoaded() {
    return (cartridge_buffer != NULL);
}

// ----------------------------------------------------------------------------
// Release
// ----------------------------------------------------------------------------
void cartridge_Release() {
    if (cartridge_buffer != NULL) {
        delete[] cartridge_buffer;
        cartridge_size = 0;
        cartridge_buffer = NULL;
    }
}

// ----------------------------------------------------------------------------
// Load (buffer)
// ----------------------------------------------------------------------------
static bool cartridge_Load(const byte* data, uint size) {
    if (size <= 128) {
        return false;
    }

    cartridge_Release();
    cartridge_ResetExtras();
    bios_bypass = false;

    byte header[128] = {0};
    for (int index = 0; index < 128; index++) {
        header[index] = data[index];
    }

    if (cartridge_CC2(header)) {
        return false;
    }

    uint offset = 0;
    if (cartridge_HasHeader(header)) {
        cartridge_ReadHeader(header);
        size -= 128;
        offset = 128;
        // The header's size field can disagree with the file (truncated or bad dump)
        if (cartridge_size > size) {
            cartridge_size = size;
        }
    }
    else {
        cartridge_size = size;
    }

    // Zero-padded so bank mappings of a short ROM never read past the buffer
    uint bufferSize = (cartridge_size > CARTRIDGE_MAX_MAPPED_SIZE) ? cartridge_size : CARTRIDGE_MAX_MAPPED_SIZE;
    cartridge_buffer = new byte[bufferSize]();
    for (uint index = 0; index < cartridge_size; index++) {
        cartridge_buffer[index] = data[index + offset];
    }

    cartridge_digest = hash_Compute(cartridge_buffer, cartridge_size);

    return true;
}

// ----------------------------------------------------------------------------
// Load (file)
// ----------------------------------------------------------------------------
bool cartridge_Load(std::string filename) {
    if (filename.empty()) {
        return false;
    }

    cartridge_Release();

    byte* data = NULL;
    uint size = archive_GetUncompressedFileSize(filename);
    if (size == 0) {
        FILE* file = fopen(filename.c_str(), "rb");
        if (file == NULL) {
            return false;
        }

        if (fseek(file, 0, SEEK_END)) {
            fclose(file);
            return false;
        }
        size = ftell(file);
        if (fseek(file, 0, SEEK_SET)) {
            fclose(file);
            return false;
        }

        data = new byte[size];
        if (fread(data, 1, size, file) != size && ferror(file)) {
            fclose(file);
            cartridge_Release();
            delete[] data;
            return false;
        }

        fclose(file);
    }
    else {
        data = new byte[size];
        archive_Uncompress(filename, data, size);
    }

    if (!cartridge_Load(data, size)) {
        delete[] data;
        return false;
    }
    if (data != NULL) {
        delete[] data;
    }

    cartridge_filename = filename;
    return true;
}
