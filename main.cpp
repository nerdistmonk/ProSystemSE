#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
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

namespace fs = std::filesystem;

constexpr int VISIBLE_WIDTH = 320;
constexpr int VISIBLE_HEIGHT = 223;
constexpr int MENU_HEIGHT = 12;
constexpr int AUDIO_SAMPLES_PER_FRAME = 524;
constexpr int16_t JOYSTICK_DEADZONE = 8000;
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

    CMD_VIDEO_ASPECT_TOGGLE,
    CMD_VIDEO_FULLSCREEN,
    CMD_VIDEO_SCREENSHOT,

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

    if (SDL_SaveBMP(surface, filename.c_str())) {
        std::cout << "Saved screenshot: " << filename << std::endl;
    }
    SDL_DestroySurface(surface);
}

SDL_AudioStream* SetupAudioStream(SDL_AudioStream* oldStream, int srcFreq, int targetFreq) {
    if (oldStream) {
        SDL_DestroyAudioStream(oldStream);
    }

    SDL_AudioSpec srcSpec;
    SDL_zero(srcSpec);
    srcSpec.format = SDL_AUDIO_U8;
    srcSpec.channels = 1;
    srcSpec.freq = srcFreq;

    SDL_AudioSpec dstSpec;
    SDL_zero(dstSpec);
    dstSpec.format = SDL_AUDIO_U8;
    dstSpec.channels = 1;
    dstSpec.freq = targetFreq;

    SDL_AudioStream* newStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &dstSpec,
        NULL,
        NULL
    );

    if (newStream) {
        SDL_SetAudioStreamFormat(newStream, &srcSpec, &dstSpec);
        SDL_ResumeAudioStreamDevice(newStream);
    }
    return newStream;
}

void ClearAudioBuffers(SDL_AudioStream* stream) {
    tia_Clear();
    pokey_Clear();
    if (stream) {
        SDL_ClearAudioStream(stream);
        byte silence[AUDIO_SAMPLES_PER_FRAME];
        std::memset(silence, 0, sizeof(silence));
        SDL_PutAudioStreamData(stream, silence, sizeof(silence));
    }
}

// Helper to load ROM with Database configuration lookup
bool LoadRomAndApplyDatabase(const std::string& path) {
    if (fs::exists("ProSystem.dat")) {
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

    int configuredTargetFreq = 44100;
    int rawSourceFreq = 31440;
    float audioVolume = 1.0f;

    SDL_AudioStream* audioStream = SetupAudioStream(nullptr, rawSourceFreq, configuredTargetFreq);

    // Initialize Database Path
    database_Initialize();

    LoadConfigHistory();

    std::string currentRomPath = "";
    if (argc >= 2) {
        currentRomPath = argv[1];
        if (LoadRomAndApplyDatabase(currentRomPath)) {
            ClearAudioBuffers(audioStream);
            SaveConfigHistory(currentRomPath);
        }
    }

    int selectedRegionMode = 0;

    std::vector<MenuCategory> menus = {
        { "File", 6, 36, {
            { "Open ROM... (O/F2)", CMD_FILE_OPEN },
            { "Close ROM",          CMD_FILE_CLOSE },
            { "Quick Save (F1)",    CMD_FILE_SAVE },
            { "Quick Load (F5)",    CMD_FILE_LOAD },
            { "Save Slot: 1 (F9)", CMD_FILE_SLOT },
            { "Exit",               CMD_FILE_EXIT }
        }},
        { "Emulation", 50, 76, {
            { "Reset (R)",          CMD_EMU_RESET },
            { "Pause/Resume (P)",   CMD_EMU_PAUSE },
            { "Region: Auto [x]",   CMD_EMU_REGION_AUTO },
            { "Region: Force NTSC", CMD_EMU_REGION_NTSC },
            { "Region: Force PAL",  CMD_EMU_REGION_PAL },
            { "Left Diff: Position B", CMD_EMU_DIFF_LEFT },
            { "Right Diff: Position B", CMD_EMU_DIFF_RIGHT }
        }},
        { "Video", 136, 44, {
            { "Aspect: 4:3 Preserved", CMD_VIDEO_ASPECT_TOGGLE },
            { "Fullscreen (F11)",      CMD_VIDEO_FULLSCREEN },
            { "Take Screenshot (F12)", CMD_VIDEO_SCREENSHOT }
        }},
        { "Audio", 186, 44, {
            { "Mute / Unmute",         CMD_AUDIO_MUTE },
            { "11025 Hz",              CMD_AUDIO_FREQ_11025 },
            { "22050 Hz",              CMD_AUDIO_FREQ_22050 },
            { "44100 Hz [x]",          CMD_AUDIO_FREQ_44100 },
            { "48000 Hz",              CMD_AUDIO_FREQ_48000 },
            { "Volume: 100%",          CMD_AUDIO_VOL_100 },
            { "Volume: 50%",           CMD_AUDIO_VOL_50 }
        }}
    };

    auto UpdateRegionLabels = [&](int mode) {
        selectedRegionMode = mode;
        menus[1].items[2].label = (mode == 0) ? "Region: Auto [x]" : "Region: Auto";
        menus[1].items[3].label = (mode == 1) ? "Region: Force NTSC [x]" : "Region: Force NTSC";
        menus[1].items[4].label = (mode == 2) ? "Region: Force PAL [x]" : "Region: Force PAL";
    };

    auto UpdateSampleRateLabels = [&](int rate) {
        configuredTargetFreq = rate;
        audioStream = SetupAudioStream(audioStream, rawSourceFreq, configuredTargetFreq);

        menus[3].items[1].label = (rate == 11025) ? "11025 Hz [x]" : "11025 Hz";
        menus[3].items[2].label = (rate == 22050) ? "22050 Hz [x]" : "22050 Hz";
        menus[3].items[3].label = (rate == 44100) ? "44100 Hz [x]" : "44100 Hz";
        menus[3].items[4].label = (rate == 48000) ? "48000 Hz [x]" : "48000 Hz";
    };

    auto TriggerOpenFileDialog = [&]() {
        static const SDL_DialogFileFilter filters[] = {
            { "Atari 7800 ROMs (*.a78, *.bin)", "a78;bin" },
            { "All Files", "*" }
        };
        const char* defaultLoc = g_lastRomDir.empty() ? NULL : g_lastRomDir.c_str();
        SDL_ShowOpenFileDialog(OpenRomCallback, NULL, window, filters, 2, defaultLoc, false);
    };

    SDL_Gamepad* gamepad1 = nullptr;
    SDL_Gamepad* gamepad2 = nullptr;

    bool running = true;
    bool isFullscreen = false;
    bool isMuted = false;
    bool leftDiffA = false;
    bool rightDiffA = false;
    int saveSlot = 1;
    int activeMenuIdx = -1;

    SDL_Event event;
    uint32_t pixels[VISIBLE_WIDTH * VISIBLE_HEIGHT];
    byte input[17] = {0};
    byte frameAudio[AUDIO_SAMPLES_PER_FRAME];

    const int visibleOffset = (maria_visibleArea.top - maria_displayArea.top) * VISIBLE_WIDTH;
    uint64_t nextFrameTime = SDL_GetTicksNS();

    while (running) {
        float mouseX = -1.0f, mouseY = -1.0f;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
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
                    if (!isFullscreen) {
                        bool clickedMenu = false;
                        if (mouseY >= 0 && mouseY <= MENU_HEIGHT) {
                            for (size_t i = 0; i < menus.size(); ++i) {
                                if (mouseX >= menus[i].x - 2 && mouseX < menus[i].x + menus[i].width) {
                                    activeMenuIdx = (activeMenuIdx == static_cast<int>(i)) ? -1 : static_cast<int>(i);
                                    clickedMenu = true;
                                    break;
                                }
                            }
                        }

                        if (!clickedMenu && activeMenuIdx >= 0) {
                            const auto& cat = menus[activeMenuIdx];
                            float dropY = MENU_HEIGHT;
                            float dropW = 180.0f;
                            float dropX = std::min(cat.x - 2, static_cast<float>(VISIBLE_WIDTH) - dropW);

                            for (size_t j = 0; j < cat.items.size(); ++j) {
                                float itemH = 10.0f;
                                if (mouseX >= dropX && mouseX <= dropX + dropW &&
                                    mouseY >= dropY && mouseY < dropY + itemH) {
                                    
                                    MenuCommand cmd = cat.items[j].cmd;
                                    if (cmd == CMD_FILE_OPEN) {
                                        TriggerOpenFileDialog();
                                    } else if (cmd == CMD_FILE_CLOSE) {
                                        prosystem_Close();
                                        ClearAudioBuffers(audioStream);
                                    } else if (cmd == CMD_FILE_SAVE && prosystem_active) {
                                        prosystem_Save("slot_" + std::to_string(saveSlot) + ".sav", true);
                                    } else if (cmd == CMD_FILE_LOAD && prosystem_active) {
                                        prosystem_Load("slot_" + std::to_string(saveSlot) + ".sav");
                                        ClearAudioBuffers(audioStream);
                                    } else if (cmd == CMD_FILE_SLOT) {
                                        saveSlot = (saveSlot % 9) + 1;
                                        menus[0].items[4].label = "Save Slot: " + std::to_string(saveSlot) + " (F9)";
                                    } else if (cmd == CMD_FILE_EXIT) {
                                        running = false;
                                    } else if (cmd == CMD_EMU_RESET && prosystem_active) {
                                        prosystem_Reset();
                                        ClearAudioBuffers(audioStream);
                                    } else if (cmd == CMD_EMU_PAUSE && prosystem_active) {
                                        prosystem_Pause(!prosystem_paused);
                                        ClearAudioBuffers(audioStream);
                                    } else if (cmd == CMD_EMU_REGION_AUTO) {
                                        UpdateRegionLabels(0);
                                    } else if (cmd == CMD_EMU_REGION_NTSC) {
                                        UpdateRegionLabels(1);
                                        prosystem_frequency = 60;
                                        rawSourceFreq = 524 * 60;
                                        audioStream = SetupAudioStream(audioStream, rawSourceFreq, configuredTargetFreq);
                                    } else if (cmd == CMD_EMU_REGION_PAL) {
                                        UpdateRegionLabels(2);
                                        prosystem_frequency = 50;
                                        rawSourceFreq = 524 * 50;
                                        audioStream = SetupAudioStream(audioStream, rawSourceFreq, configuredTargetFreq);
                                    } else if (cmd == CMD_EMU_DIFF_LEFT) {
                                        leftDiffA = !leftDiffA;
                                        menus[1].items[5].label = leftDiffA ? "Left Diff: Position A" : "Left Diff: Position B";
                                    } else if (cmd == CMD_EMU_DIFF_RIGHT) {
                                        rightDiffA = !rightDiffA;
                                        menus[1].items[6].label = rightDiffA ? "Right Diff: Position A" : "Right Diff: Position B";
                                    } else if (cmd == CMD_VIDEO_ASPECT_TOGGLE) {
                                        preserveAspect = !preserveAspect;
                                        SDL_SetRenderLogicalPresentation(
                                            renderer, VISIBLE_WIDTH, VISIBLE_HEIGHT,
                                            preserveAspect ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_STRETCH
                                        );
                                        menus[2].items[0].label = preserveAspect ? "Aspect: 4:3 Preserved" : "Aspect: Stretched Fill";
                                    } else if (cmd == CMD_VIDEO_FULLSCREEN) {
                                        isFullscreen = !isFullscreen;
                                        SDL_SetWindowFullscreen(window, isFullscreen);
                                    } else if (cmd == CMD_VIDEO_SCREENSHOT) {
                                        SaveScreenshot(renderer);
                                    } else if (cmd == CMD_AUDIO_MUTE) {
                                        isMuted = !isMuted;
                                    } else if (cmd == CMD_AUDIO_FREQ_11025) {
                                        UpdateSampleRateLabels(11025);
                                    } else if (cmd == CMD_AUDIO_FREQ_22050) {
                                        UpdateSampleRateLabels(22050);
                                    } else if (cmd == CMD_AUDIO_FREQ_44100) {
                                        UpdateSampleRateLabels(44100);
                                    } else if (cmd == CMD_AUDIO_FREQ_48000) {
                                        UpdateSampleRateLabels(48000);
                                    } else if (cmd == CMD_AUDIO_VOL_100) {
                                        audioVolume = 1.0f;
                                    } else if (cmd == CMD_AUDIO_VOL_50) {
                                        audioVolume = 0.5f;
                                    }

                                    activeMenuIdx = -1;
                                    clickedMenu = true;
                                    break;
                                }
                                dropY += itemH;
                            }
                        }

                        if (!clickedMenu) {
                            activeMenuIdx = -1;
                        }
                    }
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode key = event.key.key;

                if (key == SDLK_F11 || ((event.key.mod & SDL_KMOD_ALT) && key == SDLK_RETURN)) {
                    isFullscreen = !isFullscreen;
                    SDL_SetWindowFullscreen(window, isFullscreen);
                    activeMenuIdx = -1;
                } else if (key == SDLK_F12) {
                    SaveScreenshot(renderer);
                } else if (key == SDLK_O || key == SDLK_F2) {
                    TriggerOpenFileDialog();
                } else if (key == SDLK_F1 && prosystem_active) {
                    prosystem_Save("slot_" + std::to_string(saveSlot) + ".sav", true);
                } else if (key == SDLK_F5 && prosystem_active) {
                    prosystem_Load("slot_" + std::to_string(saveSlot) + ".sav");
                    ClearAudioBuffers(audioStream);
                } else if (key == SDLK_F9) {
                    saveSlot = (saveSlot % 9) + 1;
                    menus[0].items[4].label = "Save Slot: " + std::to_string(saveSlot) + " (F9)";
                } else if (key == SDLK_P && prosystem_active) {
                    prosystem_Pause(!prosystem_paused);
                    ClearAudioBuffers(audioStream);
                }
            } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                if (!gamepad1) gamepad1 = SDL_OpenGamepad(event.gdevice.which);
                else if (!gamepad2) gamepad2 = SDL_OpenGamepad(event.gdevice.which);
            } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                if (gamepad1 && SDL_GetGamepadID(gamepad1) == event.gdevice.which) {
                    SDL_CloseGamepad(gamepad1); gamepad1 = nullptr;
                } else if (gamepad2 && SDL_GetGamepadID(gamepad2) == event.gdevice.which) {
                    SDL_CloseGamepad(gamepad2); gamepad2 = nullptr;
                }
            }
        }

        if (g_pendingRom.pending) {
            g_pendingRom.pending = false;
            prosystem_Close();
            if (LoadRomAndApplyDatabase(g_pendingRom.path)) {
                ClearAudioBuffers(audioStream);
                currentRomPath = g_pendingRom.path;
                SaveConfigHistory(currentRomPath);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (prosystem_active && !prosystem_paused) {
            int targetFreq = (prosystem_frequency > 0) ? prosystem_frequency : 60;

            const bool* keys = SDL_GetKeyboardState(NULL);
            std::memset(input, 0, sizeof(input));

            bool right1 = keys[SDL_SCANCODE_RIGHT];
            bool left1  = keys[SDL_SCANCODE_LEFT];
            bool down1  = keys[SDL_SCANCODE_DOWN];
            bool up1    = keys[SDL_SCANCODE_UP];
            bool btn1_1 = keys[SDL_SCANCODE_Z];
            bool btn1_2 = keys[SDL_SCANCODE_X];

            if (gamepad1) {
                int16_t axisX = SDL_GetGamepadAxis(gamepad1, SDL_GAMEPAD_AXIS_LEFTX);
                int16_t axisY = SDL_GetGamepadAxis(gamepad1, SDL_GAMEPAD_AXIS_LEFTY);

                right1 |= SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || (axisX > JOYSTICK_DEADZONE);
                left1  |= SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_DPAD_LEFT)  || (axisX < -JOYSTICK_DEADZONE);
                down1  |= SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_DPAD_DOWN)  || (axisY > JOYSTICK_DEADZONE);
                up1    |= SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_DPAD_UP)    || (axisY < -JOYSTICK_DEADZONE);

                btn1_1 |= SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_SOUTH);
                btn1_2 |= SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_WEST);

                if (SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_START)) input[0x0c] = 1;
                if (SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_BACK))  input[0x0d] = 1;
            }

            input[0x00] = right1 ? 1 : 0;
            input[0x01] = left1  ? 1 : 0;
            input[0x02] = down1  ? 1 : 0;
            input[0x03] = up1    ? 1 : 0;
            input[0x04] = btn1_1 ? 1 : 0;
            input[0x05] = btn1_2 ? 1 : 0;

            input[0x0e] = leftDiffA ? 1 : 0;
            input[0x0f] = rightDiffA ? 1 : 0;

            if (keys[SDL_SCANCODE_R]) input[0x0c] = 1;
            if (keys[SDL_SCANCODE_S]) input[0x0d] = 1;

            prosystem_ExecuteFrame(input);

            if (audioStream && !isMuted) {
                for (int i = 0; i < AUDIO_SAMPLES_PER_FRAME; ++i) {
                    int16_t mixed = static_cast<int16_t>(tia_buffer[i]);
                    if (cartridge_pokey) {
                        mixed = (mixed + static_cast<int16_t>(pokey_buffer[i])) / 2;
                    }
                    mixed = static_cast<int16_t>(mixed * audioVolume);
                    frameAudio[i] = static_cast<byte>(std::clamp<int16_t>(mixed, 0, 255));
                }

                if (SDL_GetAudioStreamQueued(audioStream) < (AUDIO_SAMPLES_PER_FRAME * 2)) {
                    SDL_PutAudioStreamData(audioStream, frameAudio, sizeof(frameAudio));
                }
                SDL_PutAudioStreamData(audioStream, frameAudio, sizeof(frameAudio));
            }

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
        } else {
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDebugText(renderer, 50, 110, "Press O or F2 to Load a ROM");
            SDL_Delay(16);
        }

        // Render Menu Bar (Windowed)
        if (!isFullscreen) {
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 230);
            SDL_FRect barRect = { 0, 0, (float)VISIBLE_WIDTH, (float)MENU_HEIGHT };
            SDL_RenderFillRect(renderer, &barRect);

            for (size_t i = 0; i < menus.size(); ++i) {
                if (static_cast<int>(i) == activeMenuIdx) {
                    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
                    SDL_FRect catBg = { menus[i].x - 2, 0, menus[i].width, (float)MENU_HEIGHT };
                    SDL_RenderFillRect(renderer, &catBg);
                }
                SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
                SDL_RenderDebugText(renderer, menus[i].x, 2, menus[i].title.c_str());
            }

            if (activeMenuIdx >= 0 && activeMenuIdx < static_cast<int>(menus.size())) {
                const auto& cat = menus[activeMenuIdx];
                float dropY = MENU_HEIGHT;
                float dropW = 180.0f;
                float dropX = std::min(cat.x - 2, static_cast<float>(VISIBLE_WIDTH) - dropW);
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

        SDL_RenderPresent(renderer);
    }

    if (gamepad1) SDL_CloseGamepad(gamepad1);
    if (gamepad2) SDL_CloseGamepad(gamepad2);

    prosystem_Close();
    if (audioStream) SDL_DestroyAudioStream(audioStream);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
