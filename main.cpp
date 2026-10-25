#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <cstring>

#include "ProSystem.h"
#include "Cartridge.h"
#include "Maria.h"
#include "Palette.h"
#include "Tia.h"
#include "Pokey.h"

constexpr int VISIBLE_WIDTH = 320;
constexpr int VISIBLE_HEIGHT = 223;
constexpr int AUDIO_SAMPLES_PER_FRAME = 524;
constexpr int16_t JOYSTICK_DEADZONE = 8000;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: prosystem <path-to-rom.a78>" << std::endl;
        return 1;
    }

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

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        VISIBLE_WIDTH,
        VISIBLE_HEIGHT
    );

    SDL_AudioSpec audioSpec;
    SDL_zero(audioSpec);
    audioSpec.format = SDL_AUDIO_U8;
    audioSpec.channels = 1;
    audioSpec.freq = 31440;

    SDL_AudioStream* audioStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &audioSpec,
        NULL,
        NULL
    );

    if (audioStream) {
        SDL_ResumeAudioStreamDevice(audioStream);
    }

    if (!cartridge_Load(argv[1])) {
        std::cerr << "Failed to load cartridge: " << argv[1] << std::endl;
        if (audioStream) SDL_DestroyAudioStream(audioStream);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    prosystem_Reset();

    SDL_Gamepad* gamepad1 = nullptr;
    SDL_Gamepad* gamepad2 = nullptr;

    bool running = true;
    SDL_Event event;
    uint32_t pixels[VISIBLE_WIDTH * VISIBLE_HEIGHT];
    byte input[17] = {0};
    byte frameAudio[AUDIO_SAMPLES_PER_FRAME];

    const int visibleOffset = (maria_visibleArea.top - maria_displayArea.top) * VISIBLE_WIDTH;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                if (!gamepad1) {
                    gamepad1 = SDL_OpenGamepad(event.gdevice.which);
                    std::cout << "Gamepad 1 connected." << std::endl;
                } else if (!gamepad2) {
                    gamepad2 = SDL_OpenGamepad(event.gdevice.which);
                    std::cout << "Gamepad 2 connected." << std::endl;
                }
            } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                if (gamepad1 && SDL_GetGamepadID(gamepad1) == event.gdevice.which) {
                    SDL_CloseGamepad(gamepad1);
                    gamepad1 = nullptr;
                    std::cout << "Gamepad 1 disconnected." << std::endl;
                } else if (gamepad2 && SDL_GetGamepadID(gamepad2) == event.gdevice.which) {
                    SDL_CloseGamepad(gamepad2);
                    gamepad2 = nullptr;
                    std::cout << "Gamepad 2 disconnected." << std::endl;
                }
            }
        }

        const bool* keys = SDL_GetKeyboardState(NULL);
        std::memset(input, 0, sizeof(input));

        // Player 1 inputs (Keyboard OR Gamepad 1)
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

            btn1_1 |= SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_SOUTH); // A
            btn1_2 |= SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_WEST); // B / X

            if (SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_START)) input[0x0c] = 1; // Console Reset
            if (SDL_GetGamepadButton(gamepad1, SDL_GAMEPAD_BUTTON_BACK))  input[0x0d] = 1; // Console Select
        }

        input[0x00] = right1 ? 1 : 0;
        input[0x01] = left1  ? 1 : 0;
        input[0x02] = down1  ? 1 : 0;
        input[0x03] = up1    ? 1 : 0;
        input[0x04] = btn1_1 ? 1 : 0;
        input[0x05] = btn1_2 ? 1 : 0;

        // Player 2 inputs (Gamepad 2)
        if (gamepad2) {
            int16_t axisX2 = SDL_GetGamepadAxis(gamepad2, SDL_GAMEPAD_AXIS_LEFTX);
            int16_t axisY2 = SDL_GetGamepadAxis(gamepad2, SDL_GAMEPAD_AXIS_LEFTY);

            input[0x06] = (SDL_GetGamepadButton(gamepad2, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || (axisX2 > JOYSTICK_DEADZONE)) ? 1 : 0;
            input[0x07] = (SDL_GetGamepadButton(gamepad2, SDL_GAMEPAD_BUTTON_DPAD_LEFT)  || (axisX2 < -JOYSTICK_DEADZONE)) ? 1 : 0;
            input[0x08] = (SDL_GetGamepadButton(gamepad2, SDL_GAMEPAD_BUTTON_DPAD_DOWN)  || (axisY2 > JOYSTICK_DEADZONE)) ? 1 : 0;
            input[0x09] = (SDL_GetGamepadButton(gamepad2, SDL_GAMEPAD_BUTTON_DPAD_UP)    || (axisY2 < -JOYSTICK_DEADZONE)) ? 1 : 0;
            input[0x0a] = SDL_GetGamepadButton(gamepad2, SDL_GAMEPAD_BUTTON_SOUTH) ? 1 : 0;
            input[0x0b] = (SDL_GetGamepadButton(gamepad2, SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadButton(gamepad2, SDL_GAMEPAD_BUTTON_WEST)) ? 1 : 0;
        }

        // Console Switches
        if (keys[SDL_SCANCODE_R]) input[0x0c] = 1; // Reset
        if (keys[SDL_SCANCODE_S]) input[0x0d] = 1; // Select

        prosystem_ExecuteFrame(input);

        if (audioStream) {
            for (int i = 0; i < AUDIO_SAMPLES_PER_FRAME; ++i) {
                uint sample = tia_buffer[i];
                if (cartridge_pokey) {
                    sample = (sample + pokey_buffer[i]) / 2;
                }
                frameAudio[i] = static_cast<byte>(sample);
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
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
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
