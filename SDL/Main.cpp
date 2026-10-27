#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>

#include "ProSystem.h"
#include "Cartridge.h"
#include "Database.h"
#include "Bios.h"
#include "Audio.h"
#include "Menu.h"
#include "Input.h"
#include "Video.h"

namespace fs = std::filesystem;

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

void LoadBios(const std::string& configDir) {
    std::vector<std::string> searchPaths;

    // Location 1: ~/.config/prosystem/
    searchPaths.push_back(configDir + "/7800.rom");
    searchPaths.push_back(configDir + "/bios.rom");

    // Location 2: Executable folder
    const char* basePath = SDL_GetBasePath();
    if (basePath) {
        searchPaths.push_back(std::string(basePath) + "7800.rom");
        searchPaths.push_back(std::string(basePath) + "bios.rom");
    }

    // Working directory fallbacks
    searchPaths.push_back("7800.rom");
    searchPaths.push_back("bios.rom");
    searchPaths.push_back("../7800.rom");
    searchPaths.push_back("../bios.rom");

    for (const auto& path : searchPaths) {
        if (fs::exists(path)) {
            if (bios_Load(path)) {
                bios_enabled = true;
                std::cout << "Loaded 7800 BIOS: " << path << std::endl;
                return;
            }
        }
    }

    bios_enabled = false;
    std::cout << "No BIOS found (checked config dir and executable dir). Using fast HLE boot." << std::endl;
}

bool LoadRomAndApplyDatabase(const std::string& path) {
    static std::string dbPath;
    const char* basePath = SDL_GetBasePath();

    std::string configDb = g_configDir + "/ProSystem.dat";
    std::string exeDb = basePath ? (std::string(basePath) + "ProSystem.dat") : "";

    if (fs::exists(configDb)) {
        dbPath = configDb;
        database_filename = dbPath.c_str();
    } else if (!exeDb.empty() && fs::exists(exeDb)) {
        dbPath = exeDb;
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
    bool startFullscreen = false;
    int targetSampleRate = 44100;
    int forceRegion = 0; // 0 = Auto/ROM default, 1 = NTSC, 2 = PAL
    std::string romPathFromArgs = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-f" || arg == "--fullscreen") {
            startFullscreen = true;
        } else if ((arg == "-s" || arg == "--samplerate") && i + 1 < argc) {
            targetSampleRate = std::atoi(argv[++i]);
        } else if ((arg == "-r" || arg == "--region") && i + 1 < argc) {
            std::string reg = argv[++i];
            for (auto& c : reg) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (reg == "ntsc" || reg == "60") {
                forceRegion = 1;
            } else if (reg == "pal" || reg == "50") {
                forceRegion = 2;
            } else {
                std::cerr << "Warning: Unknown region '" << reg << "'. Use 'ntsc' or 'pal'.\n";
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: prosystem [options] [rom_path]\n\n"
                      << "Options:\n"
                      << "  -f, --fullscreen           Start in borderless fullscreen\n"
                      << "  -s, --samplerate <rate>    Set audio sample rate (11025, 22050, 44100, 48000)\n"
                      << "  -r, --region <ntsc|pal>    Force NTSC (60Hz) or PAL (50Hz) timing\n"
                      << "  -h, --help                 Show this help message\n";
            return 0;
        } else if (arg[0] != '-') {
            romPathFromArgs = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (!Video_Init("ProSystem SE - Atari 7800", 640, 446)) {
        SDL_Quit();
        return 1;
    }

    if (startFullscreen) {
        Video_ToggleFullscreen();
    }

    database_Initialize();
    LoadConfigHistory();
    LoadBios(g_configDir);

    Audio_Init(31440, targetSampleRate);
    Menu_Init();
    Menu_UpdateAudioFreqLabel(targetSampleRate);

    // Synchronize Region GUI and frequency states from CLI flags immediately
    if (forceRegion == 1) {
        prosystem_frequency = 60;
        Audio_SetSourceFrequency(524 * 60);
        Menu_UpdateRegionLabel(1);
    } else if (forceRegion == 2) {
        prosystem_frequency = 50;
        Audio_SetSourceFrequency(524 * 50);
        Menu_UpdateRegionLabel(2);
    } else {
        Menu_UpdateRegionLabel(0);
    }

    Input_Init(g_configDir);

    std::string currentRomPath = "";
    if (!romPathFromArgs.empty()) {
        currentRomPath = romPathFromArgs;
        if (LoadRomAndApplyDatabase(currentRomPath)) {
            Audio_Clear();
            SaveConfigHistory(currentRomPath);

            // Re-enforce override if provided via CLI after database_Load/reset
            if (forceRegion == 1) {
                prosystem_frequency = 60;
                Audio_SetSourceFrequency(524 * 60);
                Menu_UpdateRegionLabel(1);
            } else if (forceRegion == 2) {
                prosystem_frequency = 50;
                Audio_SetSourceFrequency(524 * 50);
                Menu_UpdateRegionLabel(2);
            }
        }
    }

    auto TriggerOpenFileDialog = [&]() {
        static const SDL_DialogFileFilter filters[] = {
            { "Atari 7800 ROMs (*.a78, *.bin, *.zip)", "a78;bin" },
            { "All Files", "*" }
        };
        const char* defaultLoc = g_lastRomDir.empty() ? NULL : g_lastRomDir.c_str();
        SDL_ShowOpenFileDialog(OpenRomCallback, NULL, Video_GetWindow(), filters, 2, defaultLoc, false);
    };

    bool running = true;
    bool leftDiffA = false;
    bool rightDiffA = false;
    int saveSlot = 1;

    SDL_Event event;
    byte input[17] = {0};
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
                SDL_ConvertEventToRenderCoordinates(Video_GetRenderer(), &event);
                if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    mouseX = event.button.x;
                    mouseY = event.button.y;
                }

                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                    MenuCommand cmd = Menu_HandleClick(mouseX, mouseY, Video_IsFullscreen(), VISIBLE_WIDTH);

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
                        forceRegion = 0;
                        Menu_UpdateRegionLabel(0);
                    } else if (cmd == CMD_EMU_REGION_NTSC) {
                        forceRegion = 1;
                        Menu_UpdateRegionLabel(1);
                        prosystem_frequency = 60;
                        Audio_SetSourceFrequency(524 * 60);
                    } else if (cmd == CMD_EMU_REGION_PAL) {
                        forceRegion = 2;
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
                        Video_ToggleAspectRatio();
                        Menu_UpdateAspectLabel(Video_IsAspectPreserved());
                    } else if (cmd == CMD_VIDEO_FULLSCREEN) {
                        Video_ToggleFullscreen();
                    } else if (cmd == CMD_VIDEO_SCREENSHOT) {
                        Video_SaveScreenshot();
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
                    Video_ToggleFullscreen();
                    Menu_SetActiveCategory(-1);
                } else if (key == SDLK_F12) {
                    Video_SaveScreenshot();
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

                if (forceRegion == 1) {
                    prosystem_frequency = 60;
                    Audio_SetSourceFrequency(524 * 60);
                    Menu_UpdateRegionLabel(1);
                } else if (forceRegion == 2) {
                    prosystem_frequency = 50;
                    Audio_SetSourceFrequency(524 * 50);
                    Menu_UpdateRegionLabel(2);
                }
            }
        }

        Video_ClearScreen();

        if (prosystem_active && !prosystem_paused && !Input_IsConfigActive()) {
            Input_CheckAutoReload();

            int targetFreq = (prosystem_frequency > 0) ? prosystem_frequency : 60;

            Input_Poll(input, leftDiffA, rightDiffA);
            prosystem_ExecuteFrame(input);
            Audio_ProcessFrame();
            Video_RenderFrame();

            uint64_t framePeriodNs = 1000000000ULL / targetFreq;
            uint64_t now = SDL_GetTicksNS();
            if (now < nextFrameTime) {
                SDL_DelayNS(nextFrameTime - now);
            }
            nextFrameTime = SDL_GetTicksNS() + framePeriodNs;
        } else if (!prosystem_active) {
            Video_RenderText(50.0f, 110.0f, "Press O or F2 to Load a ROM");
            SDL_Delay(16);
        } else if (Input_IsConfigActive()) {
            Video_RenderFrame();
            SDL_Delay(16);
        }

        // Render Menu Bar (Windowed)
        Menu_Render(Video_GetRenderer(), Video_IsFullscreen(), VISIBLE_WIDTH);

        // Gamepad Configuration In-Engine Modal Overlay
        Input_RenderConfigModal(Video_GetRenderer());

        Video_Present();
    }

    prosystem_Close();
    Input_Shutdown();
    Audio_Shutdown();
    Video_Shutdown();
    SDL_Quit();

    return 0;
}

