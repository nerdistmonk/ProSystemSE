// ----------------------------------------------------------------------------
// ProSystem.cpp
// ----------------------------------------------------------------------------
#include "ProSystem.h"
#define PRO_SYSTEM_STATE_HEADER "PRO-SYSTEM STATE"
// Header + CPU + 16K RAM + Souper registers + 32K Souper EXRAM
#define PRO_SYSTEM_STATE_MAX_SIZE 49218

bool prosystem_active = false;
bool prosystem_paused = false;
word prosystem_frequency = 60;
byte prosystem_frame = 0;
word prosystem_scanlines = 262;
uint prosystem_cycles = 0;
static uint bios_frames = 0;
// Normal BIOS handoff completes by frame ~240.
#define BIOS_HANDOFF_LIMIT 900

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
void prosystem_Reset( ) {
  if(cartridge_IsLoaded( )) {
    prosystem_paused = false;
    prosystem_frame = 0;
    region_Reset( );
    tia_Clear( );
    tia_Reset( );
    pokey_Clear( );
    pokey_Reset( );
    memory_Reset( );
    maria_Clear( );
    maria_Reset( );
	riot_Reset ( );
    bios_frames = 0;
    bios_waiting = bios_enabled && !bios_bypass && bios_IsLoaded( );
    if(bios_enabled && !bios_bypass) {
      bios_Store( );
    }
    else {
      cartridge_Store( );
    }
    prosystem_cycles = sally_ExecuteRES( );
    prosystem_active = true;
  }
}

// ----------------------------------------------------------------------------
// ExecuteFrame
// ----------------------------------------------------------------------------
void prosystem_ExecuteFrame(const byte* input) {
  bool bios_restart = false;
  riot_SetInput(input);
  
  for(maria_scanline = 1; maria_scanline <= prosystem_scanlines; maria_scanline++) {
    if(maria_scanline == maria_displayArea.top) {
      memory_ram[MSTAT] = 0;
    }
    if(maria_scanline == maria_displayArea.bottom) {
      memory_ram[MSTAT] = 128;
    }
    
    uint cycles;
    prosystem_cycles %= 456;
    while(prosystem_cycles < 28) {
      cycles = sally_ExecuteInstruction( );
      prosystem_cycles += (cycles << 2);
      if(riot_timing) {
        riot_UpdateTimer(cycles);
      }
      if(bios_waiting) {
        bios_CheckHandoff(sally_pc.w);
      }
      if(bios_Restarted(sally_pc.w)) {
        bios_bypass = true;
        bios_restart = true;
      }
      if(memory_ram[WSYNC] && !(cartridge_flags & CARTRIDGE_WSYNC_MASK)) {
        prosystem_cycles = 456;
        memory_ram[WSYNC] = false;
        break;
      }
    }
    
    cycles = maria_RenderScanline( );
    if(cartridge_flags & CARTRIDGE_CYCLE_STEALING_MASK) {
      prosystem_cycles += cycles;
    }
    
    while(prosystem_cycles < 456) {
      cycles = sally_ExecuteInstruction( );
      prosystem_cycles += (cycles << 2);
      if(riot_timing) {
        riot_UpdateTimer(cycles);
      }
      if(bios_waiting) {
        bios_CheckHandoff(sally_pc.w);
      }
      if(bios_Restarted(sally_pc.w)) {
        bios_bypass = true;
        bios_restart = true;
      }
      if(memory_ram[WSYNC] && !(cartridge_flags & CARTRIDGE_WSYNC_MASK)) {
        prosystem_cycles = 456;
        memory_ram[WSYNC] = false;
        break;
      }
    }
    tia_Process(2);
    if(cartridge_pokey || cartridge_pokey_base) {
      pokey_Process(2);
    }
  }
  if(bios_waiting && ++bios_frames > BIOS_HANDOFF_LIMIT) {
    bios_bypass = true;
    bios_restart = true;
  }
  if(bios_restart) {
    fprintf(stderr, "BIOS did not accept this cartridge (unsigned ROM); restarting without BIOS\n");
    prosystem_Reset( );
    return;
  }
  prosystem_frame++;
  if(prosystem_frame >= prosystem_frequency) {
    prosystem_frame = 0;
  }
}

// ----------------------------------------------------------------------------
// Save
// ----------------------------------------------------------------------------
bool prosystem_Save(std::string filename, bool compress) {  
    if (filename.empty()) {  
        return false;  
    }  

    byte buffer[PRO_SYSTEM_STATE_MAX_SIZE] = {0};  
    uint size = 0;  

    uint index;  
    for (index = 0; index < 16; index++) {  
        buffer[size + index] = PRO_SYSTEM_STATE_HEADER[index];  
    }  
    size += 16;  

    buffer[size++] = 1;  
    for (index = 0; index < 4; index++) {  
        buffer[size + index] = 0;  
    }  
    size += 4;  

    for (index = 0; index < 32; index++) {  
        buffer[size + index] = cartridge_digest[index];  
    }  
    size += 32;  

    buffer[size++] = sally_a;  
    buffer[size++] = sally_x;  
    buffer[size++] = sally_y;  
    buffer[size++] = sally_p;  
    buffer[size++] = sally_s;  
    buffer[size++] = sally_pc.b.l;  
    buffer[size++] = sally_pc.b.h;  
    buffer[size++] = cartridge_bank;  

    for (index = 0; index < 16384; index++) {  
        buffer[size + index] = memory_ram[index];  
    }  
    size += 16384;  

    if (cartridge_type == CARTRIDGE_TYPE_SUPERCART_RAM) {  
        for (index = 0; index < 16384; index++) {  
            buffer[size + index] = memory_ram[16384 + index];  
        }   
        size += 16384;  
    }  
    else if (cartridge_type == CARTRIDGE_TYPE_SOUPER) {  
        buffer[size++] = cartridge_souper_chr_bank[0];  
        buffer[size++] = cartridge_souper_chr_bank[1];  
        buffer[size++] = cartridge_souper_mode;  
        buffer[size++] = cartridge_souper_ram_page_bank[0];  
        buffer[size++] = cartridge_souper_ram_page_bank[1];  
        for (index = 0; index < MEMORY_SOUPER_EXRAM_SIZE; index++) {  
            buffer[size + index] = memory_souper_ram[index];  
        }  
        size += MEMORY_SOUPER_EXRAM_SIZE;  
    }  

    if (!compress) {  
        FILE* file = fopen(filename.c_str(), "wb");  
        if (file == NULL) {  
            return false;  
        }  

        if (fwrite(buffer, 1, size, file) != size) {  
            fclose(file);  
            return false;  
        }  

        fclose(file);  
    }  
    else {  
        if (!archive_Compress(filename.c_str(), "Save.sav", buffer, size)) {  
            return false;  
        }  
    }  
    return true;  
}  

// ----------------------------------------------------------------------------
// Load
// ----------------------------------------------------------------------------

bool prosystem_Load(const std::string filename) {  
    if (filename.empty()) {  
        return false;  
    }  

    byte buffer[PRO_SYSTEM_STATE_MAX_SIZE] = {0};  
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

        if (size != 16445 && size != 32829 && size != PRO_SYSTEM_STATE_MAX_SIZE) {  
            fclose(file);  
            return false;  
        }  

        if (fread(buffer, 1, size, file) != size && ferror(file)) {  
            fclose(file);  
            return false;  
        }  
        fclose(file);  
    }   
    else if (size == 16445 || size == 32829 || size == PRO_SYSTEM_STATE_MAX_SIZE) {  
        archive_Uncompress(filename, buffer, size);  
    }  
    else {  
        return false;  
    }  

    uint offset = 0;  
    uint index;  
    for (index = 0; index < 16; index++) {  
        if (buffer[offset + index] != PRO_SYSTEM_STATE_HEADER[index]) {  
            return false;  
        }  
    }  
    offset += 16;  
    byte version = buffer[offset++];  

    offset += 4;  

    char digest[33] = {0};  
    for (index = 0; index < 32; index++) {  
        digest[index] = buffer[offset + index];  
    }  
    offset += 32;  
    if (cartridge_digest != std::string(digest)) {  
        return false;  
    }  
    // Validate everything before touching the running system, so a wrong or
    // corrupt state file doesn't reset the game.
    if (cartridge_type == CARTRIDGE_TYPE_SUPERCART_RAM && size != 32829) {
        return false;
    }
    if (cartridge_type == CARTRIDGE_TYPE_SOUPER && size != PRO_SYSTEM_STATE_MAX_SIZE) {
        return false;
    }

    prosystem_Reset();  
    // A saved state is taken after the BIOS has handed off, so map the cartridge.
    bios_waiting = false;  
    cartridge_Store();  

    sally_a = buffer[offset++];  
    sally_x = buffer[offset++];  
    sally_y = buffer[offset++];  
    sally_p = buffer[offset++];  
    sally_s = buffer[offset++];  
    sally_pc.b.l = buffer[offset++];  
    sally_pc.b.h = buffer[offset++];  

    cartridge_StoreBank(buffer[offset++]);  

    for (index = 0; index < 16384; index++) {  
        memory_ram[index] = buffer[offset + index];  
    }  
    offset += 16384;  

    if (cartridge_type == CARTRIDGE_TYPE_SUPERCART_RAM) {  
        for (index = 0; index < 16384; index++) {  
            memory_ram[16384 + index] = buffer[offset + index];  
        }  
        offset += 16384;   
    }   
    else if (cartridge_type == CARTRIDGE_TYPE_SOUPER) {  
        cartridge_souper_chr_bank[0] = buffer[offset++];  
        cartridge_souper_chr_bank[1] = buffer[offset++];  
        cartridge_souper_mode = buffer[offset++];  
        cartridge_souper_ram_page_bank[0] = buffer[offset++];  
        cartridge_souper_ram_page_bank[1] = buffer[offset++];  
        for (index = 0; index < MEMORY_SOUPER_EXRAM_SIZE; index++) {  
            memory_souper_ram[index] = buffer[offset + index];  
        }  
        offset += MEMORY_SOUPER_EXRAM_SIZE;  
    }  

    return true;  
}

// ----------------------------------------------------------------------------
// Pause
// ----------------------------------------------------------------------------
void prosystem_Pause(bool pause) {
  if(prosystem_active) {
    prosystem_paused = pause;
  }
}

// ----------------------------------------------------------------------------
// Close
// ----------------------------------------------------------------------------
void prosystem_Close( ) {
  prosystem_active = false;
  prosystem_paused = false;
  cartridge_Release( );
  maria_Reset( );
  maria_Clear( );
  memory_Reset( );
  tia_Reset( );
  tia_Clear( );
}
