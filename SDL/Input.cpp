#include "Input.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fs = std::filesystem;

constexpr int16_t JOYSTICK_DEADZONE = 8000;

static std::string g_configDir = "";

// Configurable Scancodes
static SDL_Scancode p1_sc_up = SDL_SCANCODE_W;
static SDL_Scancode p1_sc_down = SDL_SCANCODE_S;
static SDL_Scancode p1_sc_left = SDL_SCANCODE_A;
static SDL_Scancode p1_sc_right = SDL_SCANCODE_D;
static SDL_Scancode p1_sc_btn1 = SDL_SCANCODE_E;
static SDL_Scancode p1_sc_btn2 = SDL_SCANCODE_Q;

static SDL_Scancode p2_sc_up = SDL_SCANCODE_UP;
static SDL_Scancode p2_sc_down = SDL_SCANCODE_DOWN;
static SDL_Scancode p2_sc_left = SDL_SCANCODE_LEFT;
static SDL_Scancode p2_sc_right = SDL_SCANCODE_RIGHT;
static SDL_Scancode p2_sc_btn1 = SDL_SCANCODE_N;
static SDL_Scancode p2_sc_btn2 = SDL_SCANCODE_M;

// Configurable Gamepad Buttons
static SDL_GamepadButton p1_btn_up = SDL_GAMEPAD_BUTTON_DPAD_UP;
static SDL_GamepadButton p1_btn_down = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
static SDL_GamepadButton p1_btn_left = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
static SDL_GamepadButton p1_btn_right = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
static SDL_GamepadButton p1_btn_fire1 = SDL_GAMEPAD_BUTTON_SOUTH;
static SDL_GamepadButton p1_btn_fire2 = SDL_GAMEPAD_BUTTON_WEST;

static SDL_GamepadButton p2_btn_up = SDL_GAMEPAD_BUTTON_DPAD_UP;
static SDL_GamepadButton p2_btn_down = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
static SDL_GamepadButton p2_btn_left = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
static SDL_GamepadButton p2_btn_right = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
static SDL_GamepadButton p2_btn_fire1 = SDL_GAMEPAD_BUTTON_SOUTH;
static SDL_GamepadButton p2_btn_fire2 = SDL_GAMEPAD_BUTTON_WEST;

// Gamepad Handles
static SDL_Gamepad* g_gamepad1 = nullptr;
static SDL_Gamepad* g_gamepad2 = nullptr;

// Gamepad Configuration Modal State
enum JoyConfigStep {
    JOY_CFG_IDLE = 0,
    JOY_CFG_UP,
    JOY_CFG_DOWN,
    JOY_CFG_LEFT,
    JOY_CFG_RIGHT,
    JOY_CFG_FIRE1,
    JOY_CFG_FIRE2
};

struct JoyConfigState {
    bool active = false;
    int player = 1;
    JoyConfigStep step = JOY_CFG_IDLE;
    SDL_GamepadButton up = SDL_GAMEPAD_BUTTON_DPAD_UP;
    SDL_GamepadButton down = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    SDL_GamepadButton left = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    SDL_GamepadButton right = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    SDL_GamepadButton b1 = SDL_GAMEPAD_BUTTON_SOUTH;
    SDL_GamepadButton b2 = SDL_GAMEPAD_BUTTON_WEST;
};

static JoyConfigState g_joyConfig;

// Keyboard Configuration Modal State
enum KeyConfigStep {
    KEY_CFG_IDLE = 0,
    KEY_CFG_UP,
    KEY_CFG_DOWN,
    KEY_CFG_LEFT,
    KEY_CFG_RIGHT,
    KEY_CFG_FIRE1,
    KEY_CFG_FIRE2
};

struct KeyConfigState {
    bool active = false;
    int player = 1;
    KeyConfigStep step = KEY_CFG_IDLE;
    SDL_Scancode up = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode down = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode left = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode right = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode b1 = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode b2 = SDL_SCANCODE_UNKNOWN;
};

static KeyConfigState g_keyConfig;

static fs::file_time_type GetFileModTime(const std::string& filepath) {
    std::error_code ec;
    if (fs::exists(filepath, ec)) {
        return fs::last_write_time(filepath, ec);
    }
    return fs::file_time_type{};
}

static void SaveJoyConfig(int player, SDL_GamepadButton up, SDL_GamepadButton down,
                          SDL_GamepadButton left, SDL_GamepadButton right,
                          SDL_GamepadButton b1, SDL_GamepadButton b2) {
    std::string filename = (player == 1) ? "joy_p1.ini" : "joy_p2.ini";
    std::ofstream out(g_configDir + "/" + filename);
    if (!out.is_open()) return;

    auto btnToStr = [](SDL_GamepadButton b, const char* defStr) {
        const char* str = SDL_GetGamepadStringForButton(b);
        return str ? str : defStr;
    };

    out << (player == 1 ? "[P1_JOY]\n" : "[P2_JOY]\n");
    out << "UP=" << btnToStr(up, "dpup") << "\n";
    out << "DOWN=" << btnToStr(down, "dpdown") << "\n";
    out << "LEFT=" << btnToStr(left, "dpleft") << "\n";
    out << "RIGHT=" << btnToStr(right, "dpright") << "\n";
    out << "BTN1=" << btnToStr(b1, "south") << "\n";
    out << "BTN2=" << btnToStr(b2, "west") << "\n";
}

static void SaveKeyConfig(int player, SDL_Scancode up, SDL_Scancode down,
                          SDL_Scancode left, SDL_Scancode right,
                          SDL_Scancode b1, SDL_Scancode b2) {
    std::string filename = (player == 1) ? "keys_p1.ini" : "keys_p2.ini";
    std::ofstream out(g_configDir + "/" + filename);
    if (!out.is_open()) return;

    auto scToStr = [](SDL_Scancode sc, const char* defStr) {
        const char* str = SDL_GetScancodeName(sc);
        return (str && str[0] != '\0') ? str : defStr;
    };

    out << (player == 1 ? "[P1_KEYS]\n" : "[P2_KEYS]\n");
    out << "UP=" << scToStr(up, "W") << "\n";
    out << "DOWN=" << scToStr(down, "S") << "\n";
    out << "LEFT=" << scToStr(left, "A") << "\n";
    out << "RIGHT=" << scToStr(right, "D") << "\n";
    out << "BTN1=" << scToStr(b1, "E") << "\n";
    out << "BTN2=" << scToStr(b2, "Q") << "\n";
}

static void LoadJoyConfig(int player, SDL_GamepadButton& up, SDL_GamepadButton& down,
                          SDL_GamepadButton& left, SDL_GamepadButton& right,
                          SDL_GamepadButton& b1, SDL_GamepadButton& b2) {
    std::string filename = (player == 1) ? "joy_p1.ini" : "joy_p2.ini";
    std::ifstream file(g_configDir + "/" + filename);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        val.erase(val.find_last_not_of(" \n\r\t") + 1);

        if (val.empty()) continue;
        SDL_GamepadButton btn = SDL_GetGamepadButtonFromString(val.c_str());
        if (btn != SDL_GAMEPAD_BUTTON_INVALID) {
            if (key == "UP") up = btn;
            else if (key == "DOWN") down = btn;
            else if (key == "LEFT") left = btn;
            else if (key == "RIGHT") right = btn;
            else if (key == "BTN1") b1 = btn;
            else if (key == "BTN2") b2 = btn;
        }
    }
}

void Input_Init(const std::string& configDir) {
    g_configDir = configDir;

    // Load custom/community gamepad mappings
    std::string userDb = g_configDir + "/gamecontrollerdb.txt";
    if (fs::exists(userDb)) {
        SDL_AddGamepadMappingsFromFile(userDb.c_str());
    } else if (fs::exists("gamecontrollerdb.txt")) {
        SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");
    } else if (fs::exists("../gamecontrollerdb.txt")) {
        SDL_AddGamepadMappingsFromFile("../gamecontrollerdb.txt");
    }

    Input_LoadControls();
    Input_EnumerateConnectedGamepads();
}

void Input_Shutdown() {
    if (g_gamepad1) {
        SDL_CloseGamepad(g_gamepad1);
        g_gamepad1 = nullptr;
    }
    if (g_gamepad2) {
        SDL_CloseGamepad(g_gamepad2);
        g_gamepad2 = nullptr;
    }
}

void Input_EnumerateConnectedGamepads() {
    int numJoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetGamepads(&numJoysticks);
    if (joysticks) {
        for (int i = 0; i < numJoysticks; ++i) {
            if (!g_gamepad1) {
                g_gamepad1 = SDL_OpenGamepad(joysticks[i]);
            } else if (!g_gamepad2) {
                g_gamepad2 = SDL_OpenGamepad(joysticks[i]);
            }
        }
        SDL_free(joysticks);
    }
}

void Input_HandleDeviceEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (!g_gamepad1) g_gamepad1 = SDL_OpenGamepad(event.gdevice.which);
        else if (!g_gamepad2) g_gamepad2 = SDL_OpenGamepad(event.gdevice.which);
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        if (g_gamepad1 && SDL_GetGamepadID(g_gamepad1) == event.gdevice.which) {
            SDL_CloseGamepad(g_gamepad1);
            g_gamepad1 = nullptr;
        } else if (g_gamepad2 && SDL_GetGamepadID(g_gamepad2) == event.gdevice.which) {
            SDL_CloseGamepad(g_gamepad2);
            g_gamepad2 = nullptr;
        }
    }
}

void Input_LoadControls() {
    auto parseIni = [](const std::string& filename, SDL_Scancode& up, SDL_Scancode& down,
                       SDL_Scancode& left, SDL_Scancode& right, SDL_Scancode& b1, SDL_Scancode& b2) {
        std::ifstream file(g_configDir + "/" + filename);
        if (!file.is_open()) return;
        std::string line;
        while (std::getline(file, line)) {
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            val.erase(val.find_last_not_of(" \n\r\t") + 1);

            if (val.empty()) continue;
            std::string scName = val;
            if (val.length() == 1) {
                char c = std::tolower(val[0]);
                if (c >= 'a' && c <= 'z') {
                    scName = std::string(1, c);
                }
            }
            SDL_Scancode sc = SDL_GetScancodeFromName(scName.c_str());
            if (sc == SDL_SCANCODE_UNKNOWN && val.length() == 1) {
                sc = SDL_GetScancodeFromKey(val[0], NULL);
            }

            if (sc != SDL_SCANCODE_UNKNOWN) {
                if (key == "UP") up = sc;
                else if (key == "DOWN") down = sc;
                else if (key == "LEFT") left = sc;
                else if (key == "RIGHT") right = sc;
                else if (key == "BTN1") b1 = sc;
                else if (key == "BTN2") b2 = sc;
            }
        }
    };

    parseIni("keys_p1.ini", p1_sc_up, p1_sc_down, p1_sc_left, p1_sc_right, p1_sc_btn1, p1_sc_btn2);
    parseIni("keys_p2.ini", p2_sc_up, p2_sc_down, p2_sc_left, p2_sc_right, p2_sc_btn1, p2_sc_btn2);

    LoadJoyConfig(1, p1_btn_up, p1_btn_down, p1_btn_left, p1_btn_right, p1_btn_fire1, p1_btn_fire2);
    LoadJoyConfig(2, p2_btn_up, p2_btn_down, p2_btn_left, p2_btn_right, p2_btn_fire1, p2_btn_fire2);
}

void Input_CheckAutoReload() {
    static auto last_p1_k_time = GetFileModTime(g_configDir + "/keys_p1.ini");
    static auto last_p2_k_time = GetFileModTime(g_configDir + "/keys_p2.ini");
    static auto last_p1_j_time = GetFileModTime(g_configDir + "/joy_p1.ini");
    static auto last_p2_j_time = GetFileModTime(g_configDir + "/joy_p2.ini");

    auto curr_p1_k_time = GetFileModTime(g_configDir + "/keys_p1.ini");
    auto curr_p2_k_time = GetFileModTime(g_configDir + "/keys_p2.ini");
    auto curr_p1_j_time = GetFileModTime(g_configDir + "/joy_p1.ini");
    auto curr_p2_j_time = GetFileModTime(g_configDir + "/joy_p2.ini");

    if (curr_p1_k_time != last_p1_k_time || curr_p2_k_time != last_p2_k_time ||
        curr_p1_j_time != last_p1_j_time || curr_p2_j_time != last_p2_j_time) {
        last_p1_k_time = curr_p1_k_time;
        last_p2_k_time = curr_p2_k_time;
        last_p1_j_time = curr_p1_j_time;
        last_p2_j_time = curr_p2_j_time;
        Input_LoadControls();
    }
}

void Input_StartJoyConfig(int player) {
    g_joyConfig.active = true;
    g_joyConfig.player = player;
    g_joyConfig.step = JOY_CFG_UP;
}

void Input_StartKeyConfig(int player) {
    g_keyConfig.active = true;
    g_keyConfig.player = player;
    g_keyConfig.step = KEY_CFG_UP;
}

bool Input_IsConfigActive() {
    return g_joyConfig.active || g_keyConfig.active;
}

bool Input_HandleConfigEvent(const SDL_Event& event) {
    if (!g_joyConfig.active && !g_keyConfig.active) return false;

    // --- Keyboard Configuration In-Engine ---
    if (g_keyConfig.active) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            SDL_Scancode sc = event.key.scancode;

            if (event.key.key == SDLK_ESCAPE) {
                g_keyConfig.active = false;
                g_keyConfig.step = KEY_CFG_IDLE;
                return true;
            }

            switch (g_keyConfig.step) {
                case KEY_CFG_UP:
                    g_keyConfig.up = sc;
                    g_keyConfig.step = KEY_CFG_DOWN;
                    break;
                case KEY_CFG_DOWN:
                    g_keyConfig.down = sc;
                    g_keyConfig.step = KEY_CFG_LEFT;
                    break;
                case KEY_CFG_LEFT:
                    g_keyConfig.left = sc;
                    g_keyConfig.step = KEY_CFG_RIGHT;
                    break;
                case KEY_CFG_RIGHT:
                    g_keyConfig.right = sc;
                    g_keyConfig.step = KEY_CFG_FIRE1;
                    break;
                case KEY_CFG_FIRE1:
                    g_keyConfig.b1 = sc;
                    g_keyConfig.step = KEY_CFG_FIRE2;
                    break;
                case KEY_CFG_FIRE2:
                    g_keyConfig.b2 = sc;
                    if (g_keyConfig.player == 1) {
                        p1_sc_up = g_keyConfig.up;
                        p1_sc_down = g_keyConfig.down;
                        p1_sc_left = g_keyConfig.left;
                        p1_sc_right = g_keyConfig.right;
                        p1_sc_btn1 = g_keyConfig.b1;
                        p1_sc_btn2 = g_keyConfig.b2;
                        SaveKeyConfig(1, p1_sc_up, p1_sc_down, p1_sc_left, p1_sc_right, p1_sc_btn1, p1_sc_btn2);
                    } else {
                        p2_sc_up = g_keyConfig.up;
                        p2_sc_down = g_keyConfig.down;
                        p2_sc_left = g_keyConfig.left;
                        p2_sc_right = g_keyConfig.right;
                        p2_sc_btn1 = g_keyConfig.b1;
                        p2_sc_btn2 = g_keyConfig.b2;
                        SaveKeyConfig(2, p2_sc_up, p2_sc_down, p2_sc_left, p2_sc_right, p2_sc_btn1, p2_sc_btn2);
                    }
                    g_keyConfig.active = false;
                    g_keyConfig.step = KEY_CFG_IDLE;
                    break;
                default:
                    break;
            }
            return true;
        }
        return true;
    }

    // --- Gamepad Configuration In-Engine ---
    if (g_joyConfig.active) {
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            SDL_GamepadButton pressedBtn = static_cast<SDL_GamepadButton>(event.gbutton.button);
            switch (g_joyConfig.step) {
                case JOY_CFG_UP:
                    g_joyConfig.up = pressedBtn;
                    g_joyConfig.step = JOY_CFG_DOWN;
                    break;
                case JOY_CFG_DOWN:
                    g_joyConfig.down = pressedBtn;
                    g_joyConfig.step = JOY_CFG_LEFT;
                    break;
                case JOY_CFG_LEFT:
                    g_joyConfig.left = pressedBtn;
                    g_joyConfig.step = JOY_CFG_RIGHT;
                    break;
                case JOY_CFG_RIGHT:
                    g_joyConfig.right = pressedBtn;
                    g_joyConfig.step = JOY_CFG_FIRE1;
                    break;
                case JOY_CFG_FIRE1:
                    g_joyConfig.b1 = pressedBtn;
                    g_joyConfig.step = JOY_CFG_FIRE2;
                    break;
                case JOY_CFG_FIRE2:
                    g_joyConfig.b2 = pressedBtn;
                    if (g_joyConfig.player == 1) {
                        p1_btn_up = g_joyConfig.up;
                        p1_btn_down = g_joyConfig.down;
                        p1_btn_left = g_joyConfig.left;
                        p1_btn_right = g_joyConfig.right;
                        p1_btn_fire1 = g_joyConfig.b1;
                        p1_btn_fire2 = g_joyConfig.b2;
                        SaveJoyConfig(1, p1_btn_up, p1_btn_down, p1_btn_left, p1_btn_right, p1_btn_fire1, p1_btn_fire2);
                    } else {
                        p2_btn_up = g_joyConfig.up;
                        p2_btn_down = g_joyConfig.down;
                        p2_btn_left = g_joyConfig.left;
                        p2_btn_right = g_joyConfig.right;
                        p2_btn_fire1 = g_joyConfig.b1;
                        p2_btn_fire2 = g_joyConfig.b2;
                        SaveJoyConfig(2, p2_btn_up, p2_btn_down, p2_btn_left, p2_btn_right, p2_btn_fire1, p2_btn_fire2);
                    }
                    g_joyConfig.active = false;
                    g_joyConfig.step = JOY_CFG_IDLE;
                    break;
                default:
                    break;
            }
            return true;
        } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
            g_joyConfig.active = false;
            g_joyConfig.step = JOY_CFG_IDLE;
            return true;
        }
        return true;
    }

    return false;
}

void Input_RenderConfigModal(SDL_Renderer* renderer) {
    if (!g_joyConfig.active && !g_keyConfig.active) return;

    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 235);
    SDL_FRect modalRect = { 30.0f, 85.0f, 260.0f, 54.0f };
    SDL_RenderFillRect(renderer, &modalRect);
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderRect(renderer, &modalRect);

    char titleBuf[64];
    const char* promptText = "";

    if (g_keyConfig.active) {
        std::snprintf(titleBuf, sizeof(titleBuf), "Configuring P%d Keyboard", g_keyConfig.player);
        switch (g_keyConfig.step) {
            case KEY_CFG_UP: promptText = "Press Key for: UP"; break;
            case KEY_CFG_DOWN: promptText = "Press Key for: DOWN"; break;
            case KEY_CFG_LEFT: promptText = "Press Key for: LEFT"; break;
            case KEY_CFG_RIGHT: promptText = "Press Key for: RIGHT"; break;
            case KEY_CFG_FIRE1: promptText = "Press Key for: BUTTON 1 (FIRE)"; break;
            case KEY_CFG_FIRE2: promptText = "Press Key for: BUTTON 2"; break;
            default: break;
        }
    } else {
        std::snprintf(titleBuf, sizeof(titleBuf), "Configuring P%d Gamepad", g_joyConfig.player);
        switch (g_joyConfig.step) {
            case JOY_CFG_UP: promptText = "Press Button for: DPAD UP"; break;
            case JOY_CFG_DOWN: promptText = "Press Button for: DPAD DOWN"; break;
            case JOY_CFG_LEFT: promptText = "Press Button for: DPAD LEFT"; break;
            case JOY_CFG_RIGHT: promptText = "Press Button for: DPAD RIGHT"; break;
            case JOY_CFG_FIRE1: promptText = "Press Button for: FIRE 1"; break;
            case JOY_CFG_FIRE2: promptText = "Press Button for: FIRE 2"; break;
            default: break;
        }
    }

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDebugText(renderer, 40.0f, 92.0f, titleBuf);

    SDL_SetRenderDrawColor(renderer, 255, 220, 0, 255);
    SDL_RenderDebugText(renderer, 40.0f, 108.0f, promptText);

    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_RenderDebugText(renderer, 40.0f, 124.0f, "[ESC] to Cancel");
}

void Input_Poll(unsigned char input[17], bool leftDiffA, bool rightDiffA) {
    const bool* keys = SDL_GetKeyboardState(NULL);
    std::memset(input, 0, 17);

    // Player 1
    bool right1 = keys[p1_sc_right];
    bool left1 = keys[p1_sc_left];
    bool down1 = keys[p1_sc_down];
    bool up1 = keys[p1_sc_up];
    bool btn1_1 = keys[p1_sc_btn1];
    bool btn1_2 = keys[p1_sc_btn2];

    if (g_gamepad1) {
        int16_t axisX = SDL_GetGamepadAxis(g_gamepad1, SDL_GAMEPAD_AXIS_LEFTX);
        int16_t axisY = SDL_GetGamepadAxis(g_gamepad1, SDL_GAMEPAD_AXIS_LEFTY);

        right1 |= SDL_GetGamepadButton(g_gamepad1, p1_btn_right) || (axisX > JOYSTICK_DEADZONE);
        left1 |= SDL_GetGamepadButton(g_gamepad1, p1_btn_left) || (axisX < -JOYSTICK_DEADZONE);
        down1 |= SDL_GetGamepadButton(g_gamepad1, p1_btn_down) || (axisY > JOYSTICK_DEADZONE);
        up1 |= SDL_GetGamepadButton(g_gamepad1, p1_btn_up) || (axisY < -JOYSTICK_DEADZONE);

        btn1_1 |= SDL_GetGamepadButton(g_gamepad1, p1_btn_fire1);
        btn1_2 |= SDL_GetGamepadButton(g_gamepad1, p1_btn_fire2);
    }

    input[0x00] = right1 ? 1 : 0;
    input[0x01] = left1 ? 1 : 0;
    input[0x02] = down1 ? 1 : 0;
    input[0x03] = up1 ? 1 : 0;
    input[0x04] = btn1_1 ? 1 : 0;
    input[0x05] = btn1_2 ? 1 : 0;

    // Player 2
    bool right2 = keys[p2_sc_right];
    bool left2 = keys[p2_sc_left];
    bool down2 = keys[p2_sc_down];
    bool up2 = keys[p2_sc_up];
    bool btn2_1 = keys[p2_sc_btn1];
    bool btn2_2 = keys[p2_sc_btn2];

    if (g_gamepad2) {
        int16_t axisX2 = SDL_GetGamepadAxis(g_gamepad2, SDL_GAMEPAD_AXIS_LEFTX);
        int16_t axisY2 = SDL_GetGamepadAxis(g_gamepad2, SDL_GAMEPAD_AXIS_LEFTY);

        right2 |= SDL_GetGamepadButton(g_gamepad2, p2_btn_right) || (axisX2 > JOYSTICK_DEADZONE);
        left2 |= SDL_GetGamepadButton(g_gamepad2, p2_btn_left) || (axisX2 < -JOYSTICK_DEADZONE);
        down2 |= SDL_GetGamepadButton(g_gamepad2, p2_btn_down) || (axisY2 > JOYSTICK_DEADZONE);
        up2 |= SDL_GetGamepadButton(g_gamepad2, p2_btn_up) || (axisY2 < -JOYSTICK_DEADZONE);

        btn2_1 |= SDL_GetGamepadButton(g_gamepad2, p2_btn_fire1);
        btn2_2 |= SDL_GetGamepadButton(g_gamepad2, p2_btn_fire2);
    }

    input[0x06] = right2 ? 1 : 0;
    input[0x07] = left2 ? 1 : 0;
    input[0x08] = down2 ? 1 : 0;
    input[0x09] = up2 ? 1 : 0;
    input[0x0a] = btn2_1 ? 1 : 0;
    input[0x0b] = btn2_2 ? 1 : 0;

    // Console Switches & RIOT Offsets (0x0C: Reset, 0x0D: Select, 0x0E: Pause, 0x0F: Left Diff, 0x10: Right Diff)
    bool resetPressed = keys[SDL_SCANCODE_Z] || (g_gamepad1 && SDL_GetGamepadButton(g_gamepad1, SDL_GAMEPAD_BUTTON_START));
    bool selectPressed = keys[SDL_SCANCODE_X] || (g_gamepad1 && SDL_GetGamepadButton(g_gamepad1, SDL_GAMEPAD_BUTTON_BACK));

    input[0x0c] = resetPressed ? 1 : 0;
    input[0x0d] = selectPressed ? 1 : 0;
    input[0x0e] = 0;
    input[0x0f] = leftDiffA ? 1 : 0;
    input[0x10] = rightDiffA ? 1 : 0;
}
