#include "Menu.h"
#include <algorithm>

static int g_activeMenuIdx = -1;
static std::vector<MenuCategory> g_menus;

void Menu_Init() {
    g_activeMenuIdx = -1;
    g_menus = {
        { "File", 6, 36, {
            { "Open ROM... (O/F2)", CMD_FILE_OPEN },
            { "Close ROM", CMD_FILE_CLOSE },
            { "Quick Save (F1)", CMD_FILE_SAVE },
            { "Quick Load (F5)", CMD_FILE_LOAD },
            { "Save Slot: 1 (F9)", CMD_FILE_SLOT },
            { "Exit", CMD_FILE_EXIT }
        }},
        { "Emulation", 46, 76, {
            { "Reset (R)", CMD_EMU_RESET },
            { "Pause/Resume (P)", CMD_EMU_PAUSE },
            { "Region: Auto [x]", CMD_EMU_REGION_AUTO },
            { "Region: Force NTSC", CMD_EMU_REGION_NTSC },
            { "Region: Force PAL", CMD_EMU_REGION_PAL },
            { "Left Diff: Position B", CMD_EMU_DIFF_LEFT },
            { "Right Diff: Position B", CMD_EMU_DIFF_RIGHT }
        }},
        { "Controls", 126, 72, {
            { "P1 - Keys", CMD_CONTROLS_P1_KEYS },
            { "P1 - Joy", CMD_CONTROLS_P1_JOY },
            { "P2 - Keys", CMD_CONTROLS_P2_KEYS },
            { "P2 - Joy", CMD_CONTROLS_P2_JOY }
        }},
        { "Video", 202, 44, {
            { "Aspect: 4:3 Preserved", CMD_VIDEO_ASPECT_TOGGLE },
            { "Fullscreen (F11)", CMD_VIDEO_FULLSCREEN },
            { "Take Screenshot (F12)", CMD_VIDEO_SCREENSHOT }
        }},
        { "Audio", 250, 44, {
            { "Mute / Unmute", CMD_AUDIO_MUTE },
            { "11025 Hz", CMD_AUDIO_FREQ_11025 },
            { "22050 Hz", CMD_AUDIO_FREQ_22050 },
            { "44100 Hz [x]", CMD_AUDIO_FREQ_44100 },
            { "48000 Hz", CMD_AUDIO_FREQ_48000 },
            { "Volume: 100%", CMD_AUDIO_VOL_100 },
            { "Volume: 50%", CMD_AUDIO_VOL_50 }
        }}
    };
}

int Menu_GetActiveCategory() {
    return g_activeMenuIdx;
}

void Menu_SetActiveCategory(int idx) {
    g_activeMenuIdx = idx;
}

MenuCommand Menu_HandleClick(float mouseX, float mouseY, bool isFullscreen, int visibleWidth) {
    if (isFullscreen) return CMD_NONE;

    bool clickedCategory = false;
    if (mouseY >= 0 && mouseY <= MENU_HEIGHT) {
        for (size_t i = 0; i < g_menus.size(); ++i) {
            if (mouseX >= g_menus[i].x - 2 && mouseX < g_menus[i].x + g_menus[i].width) {
                g_activeMenuIdx = (g_activeMenuIdx == static_cast<int>(i)) ? -1 : static_cast<int>(i);
                clickedCategory = true;
                return CMD_NONE;
            }
        }
    }

    if (!clickedCategory && g_activeMenuIdx >= 0 && g_activeMenuIdx < static_cast<int>(g_menus.size())) {
        const auto& cat = g_menus[g_activeMenuIdx];
        float dropY = MENU_HEIGHT;
        float dropW = 180.0f;
        float dropX = std::min(cat.x - 2, static_cast<float>(visibleWidth) - dropW);

        for (size_t j = 0; j < cat.items.size(); ++j) {
            float itemH = 10.0f;
            if (mouseX >= dropX && mouseX <= dropX + dropW &&
                mouseY >= dropY && mouseY < dropY + itemH) {
                MenuCommand cmd = cat.items[j].cmd;
                g_activeMenuIdx = -1;
                return cmd;
            }
            dropY += itemH;
        }
    }

    if (!clickedCategory) {
        g_activeMenuIdx = -1;
    }

    return CMD_NONE;
}

void Menu_UpdateRegionLabel(int mode) {
    if (g_menus.size() > 1) {
        g_menus[1].items[2].label = (mode == 0) ? "Region: Auto [x]" : "Region: Auto";
        g_menus[1].items[3].label = (mode == 1) ? "Region: Force NTSC [x]" : "Region: Force NTSC";
        g_menus[1].items[4].label = (mode == 2) ? "Region: Force PAL [x]" : "Region: Force PAL";
    }
}

void Menu_UpdateAudioFreqLabel(int rate) {
    if (g_menus.size() > 4) {
        g_menus[4].items[1].label = (rate == 11025) ? "11025 Hz [x]" : "11025 Hz";
        g_menus[4].items[2].label = (rate == 22050) ? "22050 Hz [x]" : "22050 Hz";
        g_menus[4].items[3].label = (rate == 44100) ? "44100 Hz [x]" : "44100 Hz";
        g_menus[4].items[4].label = (rate == 48000) ? "48000 Hz [x]" : "48000 Hz";
    }
}

void Menu_UpdateSaveSlotLabel(int slot) {
    if (!g_menus.empty() && g_menus[0].items.size() > 4) {
        g_menus[0].items[4].label = "Save Slot: " + std::to_string(slot) + " (F9)";
    }
}

void Menu_UpdateAspectLabel(bool preserved) {
    if (g_menus.size() > 3 && !g_menus[3].items.empty()) {
        g_menus[3].items[0].label = preserved ? "Aspect: 4:3 Preserved" : "Aspect: Stretched Fill";
    }
}

void Menu_UpdateDiffLeftLabel(bool isA) {
    if (g_menus.size() > 1 && g_menus[1].items.size() > 5) {
        g_menus[1].items[5].label = isA ? "Left Diff: Position A" : "Left Diff: Position B";
    }
}

void Menu_UpdateDiffRightLabel(bool isA) {
    if (g_menus.size() > 1 && g_menus[1].items.size() > 6) {
        g_menus[1].items[6].label = isA ? "Right Diff: Position A" : "Right Diff: Position B";
    }
}

void Menu_Render(SDL_Renderer* renderer, bool isFullscreen, int visibleWidth) {
    if (isFullscreen) return;

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 230);
    SDL_FRect barRect = { 0, 0, (float)visibleWidth, (float)MENU_HEIGHT };
    SDL_RenderFillRect(renderer, &barRect);

    for (size_t i = 0; i < g_menus.size(); ++i) {
        if (static_cast<int>(i) == g_activeMenuIdx) {
            SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
            SDL_FRect catBg = { g_menus[i].x - 2, 0, g_menus[i].width, (float)MENU_HEIGHT };
            SDL_RenderFillRect(renderer, &catBg);
        }
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderDebugText(renderer, g_menus[i].x, 2, g_menus[i].title.c_str());
    }

    if (g_activeMenuIdx >= 0 && g_activeMenuIdx < static_cast<int>(g_menus.size())) {
        const auto& cat = g_menus[g_activeMenuIdx];
        float dropY = MENU_HEIGHT;
        float dropW = 180.0f;
        float dropX = std::min(cat.x - 2, static_cast<float>(visibleWidth) - dropW);
        float dropH = cat.items.size() * 10.0f + 2.0f;

        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 245);
        SDL_FRect dropBg = { dropX, dropY, dropW, dropH };
        SDL_RenderFillRect(renderer, &dropBg);
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderRect(renderer, &dropBg);

        for (size_t j = 0; j < cat.items.size(); ++j) {
            SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
            SDL_RenderDebugText(renderer, dropX + 4, dropY + 2 + (j * 10.0f), cat.items[j].label.c_str());
        }
    }
}
