#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

#include "ProSystem.h"
#include "Cartridge.h"
#include "Maria.h"
#include "Palette.h"
#include "Tia.h"
#include "Pokey.h"
#include "Database.h"
#include "Audio.h"
#include "Menu.h"
#include "Input.h"

namespace fs = std::filesystem;

constexpr int VISIBLE_WIDTH = 320;
constexpr int VISIBLE_HEIGHT = 223;
constexpr size_t MAX_RECENT_ROMS = 5;

struct PendingRom {
    bool pending = false;
    std::string path;
};

static PendingRom g_pendingRom;

static void SDLCALL OpenRomCallback(void* userdata, const char* const* filelist, int filter) {
    if (filelist && *filelist) {
        g_pendingRom.path = *filelist;
        g_pendingRom.pending = true;
    }
}

static std::vector<std::string> g_recentRoms;
static std::string g_configDir;
static std::string g_lastRomDir = "";

void LoadConfigHistory() {
    const char* home = std::getenv("HOME");
    if (!home) return;
    g_configDir = std::string(home) + "/.config/prosystem";
    fs::create_directories(g_configDir);

    std::ifstream infile(g_configDir + "/history.txt");
    std::string line;
    g_recentRoms.clear();
    while (std::getline(infile, line)) {
        if (!line.empty() && fs::exists(line)) {
            g_recentRoms.push_back(line);
            if (g_recentRoms.size() >= MAX_RECENT_ROMS) break;
        }
    }

    std::ifstream dirFile(g_configDir + "/lastdir.txt");
    if (std::getline(dirFile, line) && fs::exists(line)) {
        g_lastRomDir = line;
    }
}

void SaveConfigHistory(const std::string& newPath) {
    if (newPath.empty()) return;

    fs::path p(newPath);
    if (fs::exists(p)) {
        g_lastRomDir = p.parent_path().string();
        std::ofstream dirFile(g_configDir + "/lastdir.txt");
        dirFile << g_lastRomDir << "\n";
    }

    auto it = std::find(g_recentRoms.begin(), g_recentRoms.end(), newPath);
    if (it != g_recentRoms.end()) {
        g_recentRoms.erase(it);
    }
    g_recentRoms.insert(g_recentRoms.begin(), newPath);
    if (g_recentRoms.size() > MAX_RECENT_ROMS) {
        g_recentRoms.pop_back();
    }

    std::ofstream outfile(g_configDir + "/history.txt");
    for (const auto& path : g_recentRoms) {
        outfile << path << "\n";
    }
}

void SaveScreenshot(SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, NULL);
    if (!surface) return;

    int idx = 1;
    std::string filename;
    while (true) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "screenshot_%03d.png", idx++);
        filename = buf;
        if (!fs::exists(filename)) break;
    }

    if (SDL_SavePNG(surface, filename.c_str())) {
        std::cout << "Saved screenshot: " << filename << std::endl;
    }
    SDL_DestroySurface(surface);
}

bool LoadRomAndApplyDatabase(const std::string& path) {
    static std::string dbPath;

    std::string configDb = g_configDir + "/ProSystem.dat";
    if (fs::exists(configDb)) {
        dbPath = configDb;
        database_filename = dbPath.c_str();
    } else if (fs::exists("ProSystem.dat")) {
        database_filename = "ProSystem.dat";
    } else if (fs::exists("../ProSystem.dat")) {
        database_filename = "../ProSystem.dat";
    }

    if (cartridge_Load(path)) {
        database_Load(cartridge_digest);
        prosystem_Reset();
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("ProSystem SE - Atari 7800", 640, 446, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 1);
    bool preserveAspect = true;
    SDL_SetRenderLogicalPresentation(renderer, VISIBLE_WIDTH, VISIBLE_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, VISIBLE_WIDTH, VISIBLE_HEIGHT);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    database_Initialize();
    LoadConfigHistory();

    Audio_Init(31440, 44100);
    Menu_Init();
    Input_Init(g_configDir);

    std::string currentRomPath = "";
    if (argc >= 2) {
        currentRomPath = argv[1];
        if (LoadRomAndApplyDatabase(currentRomPath)) {
            Audio_Clear();
            SaveConfigHistory(currentRomPath);
        }
    }

    auto TriggerOpenFileDialog = [&]() {
        static const SDL_DialogFileFilter filters[] = {
            { "Atari 7800 ROMs (*.a78, *.bin, *.zip)", "a78;bin" },
            { "All Files", "*" }
        };
        const char* defaultLoc = g_lastRomDir.empty() ? NULL : g_lastRomDir.c_str();
        SDL_ShowOpenFileDialog(OpenRomCallback, NULL, window, filters, 2, defaultLoc, false);
    };

    bool running = true;
    bool isFullscreen = false;
    bool leftDiffA = false;
    bool rightDiffA = false;
    int saveSlot = 1;

    SDL_Event event;
    uint32_t pixels[VISIBLE_WIDTH * VISIBLE_HEIGHT];
    byte input[17] = {0};

    const int visibleOffset = (maria_visibleArea.top - maria_displayArea.top) * VISIBLE_WIDTH;
    uint64_t nextFrameTime = SDL_GetTicksNS();

    while (running) {
        float mouseX = -1.0f, mouseY = -1.0f;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }

            // Intercept modal gamepad configuration if active
            if (Input_HandleConfigEvent(event)) {
                continue;
            }

            if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                Input_HandleDeviceEvent(event);
            } else if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                SDL_ConvertEventToRenderCoordinates(renderer, &event);
                if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    mouseX = event.button.x;
                    mouseY = event.button.y;
                }

                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                    MenuCommand cmd = Menu_HandleClick(mouseX, mouseY, isFullscreen, VISIBLE_WIDTH);

                    if (cmd == CMD_FILE_OPEN) {
                        TriggerOpenFileDialog();
                    } else if (cmd == CMD_FILE_CLOSE) {
                        prosystem_Close();
                        Audio_Clear();
                    } else if (cmd == CMD_FILE_SAVE && prosystem_active) {
                        prosystem_Save("slot_" + std::to_string(saveSlot) + ".sav", true);
                    } else if (cmd == CMD_FILE_LOAD && prosystem_active) {
                        prosystem_Load("slot_" + std::to_string(saveSlot) + ".sav");
                        Audio_Clear();
                    } else if (cmd == CMD_FILE_SLOT) {
                        saveSlot = (saveSlot % 9) + 1;
                        Menu_UpdateSaveSlotLabel(saveSlot);
                    } else if (cmd == CMD_FILE_EXIT) {
                        running = false;
                    } else if (cmd == CMD_EMU_RESET && prosystem_active) {
                        prosystem_Reset();
                        Audio_Clear();
                    } else if (cmd == CMD_EMU_PAUSE && prosystem_active) {
                        prosystem_Pause(!prosystem_paused);
                        Audio_Clear();
                    } else if (cmd == CMD_EMU_REGION_AUTO) {
                        Menu_UpdateRegionLabel(0);
                    } else if (cmd == CMD_EMU_REGION_NTSC) {
                        Menu_UpdateRegionLabel(1);
                        prosystem_frequency = 60;
                        Audio_SetSourceFrequency(524 * 60);
                    } else if (cmd == CMD_EMU_REGION_PAL) {
                        Menu_UpdateRegionLabel(2);
                        prosystem_frequency = 50;
                        Audio_SetSourceFrequency(524 * 50);
                    } else if (cmd == CMD_EMU_DIFF_LEFT) {
                        leftDiffA = !leftDiffA;
                        Menu_UpdateDiffLeftLabel(leftDiffA);
                    } else if (cmd == CMD_EMU_DIFF_RIGHT) {
                        rightDiffA = !rightDiffA;
                        Menu_UpdateDiffRightLabel(rightDiffA);
                    } else if (cmd == CMD_CONTROLS_P1_KEYS) {
                        Input_LaunchKeyConfig(1);
                    } else if (cmd == CMD_CONTROLS_P1_JOY) {
                        Input_StartJoyConfig(1);
                    } else if (cmd == CMD_CONTROLS_P2_KEYS) {
                        Input_LaunchKeyConfig(2);
                    } else if (cmd == CMD_CONTROLS_P2_JOY) {
                        Input_StartJoyConfig(2);
                    } else if (cmd == CMD_VIDEO_ASPECT_TOGGLE) {
                        preserveAspect = !preserveAspect;
                        SDL_SetRenderLogicalPresentation(
                            renderer, VISIBLE_WIDTH, VISIBLE_HEIGHT,
                            preserveAspect ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_STRETCH
                        );
                        Menu_UpdateAspectLabel(preserveAspect);
                    } else if (cmd == CMD_VIDEO_FULLSCREEN) {
                        isFullscreen = !isFullscreen;
                        SDL_SetWindowFullscreen(window, isFullscreen);
                    } else if (cmd == CMD_VIDEO_SCREENSHOT) {
                        SaveScreenshot(renderer);
                    } else if (cmd == CMD_AUDIO_MUTE) {
                        Audio_ToggleMute();
                    } else if (cmd == CMD_AUDIO_FREQ_11025) {
                        Audio_SetTargetFrequency(11025);
                        Menu_UpdateAudioFreqLabel(11025);
                    } else if (cmd == CMD_AUDIO_FREQ_22050) {
                        Audio_SetTargetFrequency(22050);
                        Menu_UpdateAudioFreqLabel(22050);
                    } else if (cmd == CMD_AUDIO_FREQ_44100) {
                        Audio_SetTargetFrequency(44100);
                        Menu_UpdateAudioFreqLabel(44100);
                    } else if (cmd == CMD_AUDIO_FREQ_48000) {
                        Audio_SetTargetFrequency(48000);
                        Menu_UpdateAudioFreqLabel(48000);
                    } else if (cmd == CMD_AUDIO_VOL_100) {
                        Audio_SetVolume(1.0f);
                    } else if (cmd == CMD_AUDIO_VOL_50) {
                        Audio_SetVolume(0.5f);
                    }
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode key = event.key.key;

                if (key == SDLK_F11 || ((event.key.mod & SDL_KMOD_ALT) && key == SDLK_RETURN)) {
                    isFullscreen = !isFullscreen;
                    SDL_SetWindowFullscreen(window, isFullscreen);
                    Menu_SetActiveCategory(-1);
                } else if (key == SDLK_F12) {
                    SaveScreenshot(renderer);
                } else if (key == SDLK_O || key == SDLK_F2) {
                    TriggerOpenFileDialog();
                } else if (key == SDLK_F1 && prosystem_active) {
                    prosystem_Save("slot_" + std::to_string(saveSlot) + ".sav", true);
                } else if (key == SDLK_F5 && prosystem_active) {
                    prosystem_Load("slot_" + std::to_string(saveSlot) + ".sav");
                    Audio_Clear();
                } else if (key == SDLK_F9) {
                    saveSlot = (saveSlot % 9) + 1;
                    Menu_UpdateSaveSlotLabel(saveSlot);
                } else if (key == SDLK_P && prosystem_active) {
                    prosystem_Pause(!prosystem_paused);
                    Audio_Clear();
                }
            }
        }

        if (g_pendingRom.pending) {
            g_pendingRom.pending = false;
            prosystem_Close();
            if (LoadRomAndApplyDatabase(g_pendingRom.path)) {
                Audio_Clear();
                currentRomPath = g_pendingRom.path;
                SaveConfigHistory(currentRomPath);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (prosystem_active && !prosystem_paused && !Input_IsConfigActive()) {
            Input_CheckAutoReload();

            int targetFreq = (prosystem_frequency > 0) ? prosystem_frequency : 60;

            Input_Poll(input, leftDiffA, rightDiffA);

            prosystem_ExecuteFrame(input);

            Audio_ProcessFrame();

            for (int i = 0; i < VISIBLE_WIDTH * VISIBLE_HEIGHT; ++i) {
                byte colorIdx = maria_surface[visibleOffset + i];
                byte r = palette_data[colorIdx * 3 + 0];
                byte g = palette_data[colorIdx * 3 + 1];
                byte b = palette_data[colorIdx * 3 + 2];
                pixels[i] = (0xFF000000) | (r << 16) | (g << 8) | b;
            }

            SDL_UpdateTexture(texture, NULL, pixels, VISIBLE_WIDTH * sizeof(uint32_t));
            SDL_RenderTexture(renderer, texture, NULL, NULL);

            uint64_t framePeriodNs = 1000000000ULL / targetFreq;
            uint64_t now = SDL_GetTicksNS();
            if (now < nextFrameTime) {
                SDL_DelayNS(nextFrameTime - now);
            }
            nextFrameTime = SDL_GetTicksNS() + framePeriodNs;
        } else if (!prosystem_active) {
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDebugText(renderer, 50, 110, "Press O or F2 to Load a ROM");
            SDL_Delay(16);
        } else if (Input_IsConfigActive()) {
            SDL_RenderTexture(renderer, texture, NULL, NULL);
            SDL_Delay(16);
        }

        // Render Menu Bar (Windowed)
        Menu_Render(renderer, isFullscreen, VISIBLE_WIDTH);

        // Gamepad Configuration In-Engine Modal Overlay
        Input_RenderConfigModal(renderer);

        SDL_RenderPresent(renderer);
    }

    prosystem_Close();
    Input_Shutdown();
    Audio_Shutdown();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
