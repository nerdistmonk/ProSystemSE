#ifndef PROSYSTEM_SDL_MENU_H
#define PROSYSTEM_SDL_MENU_H

#include <SDL3/SDL.h>
#include <vector>
#include <string>

constexpr int MENU_HEIGHT = 12;

enum MenuCommand {
    CMD_NONE = 0,
    CMD_FILE_OPEN,
    CMD_FILE_CLOSE,
    CMD_FILE_SAVE,
    CMD_FILE_LOAD,
    CMD_FILE_SLOT,
    CMD_FILE_EXIT,

    CMD_EMU_RESET,
    CMD_EMU_PAUSE,
    CMD_EMU_REGION_AUTO,
    CMD_EMU_REGION_NTSC,
    CMD_EMU_REGION_PAL,
    CMD_EMU_DIFF_LEFT,
    CMD_EMU_DIFF_RIGHT,

    CMD_CONTROLS_P1_KEYS,
    CMD_CONTROLS_P1_JOY,
    CMD_CONTROLS_P2_KEYS,
    CMD_CONTROLS_P2_JOY,

    CMD_VIDEO_ASPECT_TOGGLE,
    CMD_VIDEO_FULLSCREEN,
    CMD_VIDEO_SCREENSHOT,
    CMD_VIDEO_FILTER_XBRZ,

    CMD_AUDIO_MUTE,
    CMD_AUDIO_FREQ_11025,
    CMD_AUDIO_FREQ_22050,
    CMD_AUDIO_FREQ_44100,
    CMD_AUDIO_FREQ_48000,
    CMD_AUDIO_VOL_100,
    CMD_AUDIO_VOL_50
};

struct MenuItem {
    std::string label;
    MenuCommand cmd;
};

struct MenuCategory {
    std::string title;
    float x;
    float width;
    std::vector<MenuItem> items;
};

// Lifecycle & State
void Menu_Init();
int  Menu_GetActiveCategory();
void Menu_SetActiveCategory(int idx);

// Mouse Handling: returns triggered MenuCommand (or CMD_NONE)
MenuCommand Menu_HandleClick(float mouseX, float mouseY, bool isFullscreen, int visibleWidth);

// Label Updaters
void Menu_UpdateRegionLabel(int mode);
void Menu_UpdateAudioFreqLabel(int rate);
void Menu_UpdateSaveSlotLabel(int slot);
void Menu_UpdateAspectLabel(bool preserved);
void Menu_UpdateFilterLabel(bool enabled);
void Menu_UpdateDiffLeftLabel(bool isA);
void Menu_UpdateDiffRightLabel(bool isA);

// Rendering
void Menu_Render(SDL_Renderer* renderer, bool isFullscreen, int visibleWidth);

#endif // PROSYSTEM_SDL_MENU_H
