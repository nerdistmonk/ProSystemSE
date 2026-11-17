#include "Bios.h"

bool bios_enabled = false;
// Set when the BIOS rejects the cartridge (unsigned ROM) and we boot without it.
bool bios_bypass = false;
std::string bios_filename;

static byte* bios_data = NULL;
static word bios_size = 0;

bool bios_Load(std::string filename) {
    if (filename.empty()) {
        return false;
    }

    bios_Release();

    bios_size = archive_GetUncompressedFileSize(filename);
    if (bios_size == 0) {
        FILE* file = fopen(filename.c_str(), "rb");
        if (file == NULL) {
            return false;
        }

        if (fseek(file, 0, SEEK_END)) {
            fclose(file);
            return false;
        }

        bios_size = ftell(file);
        if (fseek(file, 0, SEEK_SET)) {
            fclose(file);
            return false;
        }

        bios_data = new byte[bios_size];
        if (fread(bios_data, 1, bios_size, file) != bios_size && ferror(file)) {
            fclose(file);
            bios_Release();
            return false;
        }

        fclose(file);
    } else {
        bios_data = new byte[bios_size];
        archive_Uncompress(filename, bios_data, bios_size);
    }

    bios_filename = filename;
    return true;
}

bool bios_IsLoaded() {
    return (bios_data != NULL);
}

void bios_Release() {
    if (bios_data) {
        delete[] bios_data;
        bios_size = 0;
        bios_data = NULL;
    }
}

// The BIOS restarts itself when its signature check fails. A running BIOS never
// jumps back to its own entry point otherwise, so seeing it again means rejection.
bool bios_Restarted(word pc) {
    if (bios_data == NULL || !bios_enabled || bios_bypass || bios_size < 4) {
        return false;
    }
    word base = 65536 - bios_size;
    word entry = bios_data[bios_size - 4] | (bios_data[bios_size - 3] << 8);
    if (pc != entry || pc < base || (uint)(pc - base) + 8 > bios_size) {
        return false;
    }
    for (int index = 0; index < 8; index++) {
        if (memory_ram[pc + index] != bios_data[pc - base + index]) {
            return false;
        }
    }
    return true;
}

void bios_Store() {
    if (bios_data != NULL && bios_enabled && !bios_bypass) {
        memory_WriteROM(65536 - bios_size, bios_size, bios_data);
    }
}
