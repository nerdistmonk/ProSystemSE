#ifndef PROSYSTEM_SDL_INPUT_H
#define PROSYSTEM_SDL_INPUT_H

#include <SDL3/SDL.h>
#include <string>

// Lifecycle & Init
void Input_Init(const std::string& configDir);
void Input_Shutdown();

// Device hotplugging & enumeration
void Input_EnumerateConnectedGamepads();
void Input_HandleDeviceEvent(const SDL_Event& event);

// INI file persistence & hot-reload check
void Input_LoadControls();
void Input_CheckAutoReload();

// In-engine Gamepad Configuration Modal
void Input_StartJoyConfig(int player);
bool Input_IsConfigActive();
bool Input_HandleConfigEvent(const SDL_Event& event);
void Input_RenderConfigModal(SDL_Renderer* renderer);

// Terminal launcher for keyboard configuration
void Input_LaunchKeyConfig(int player);

// Frame polling: fills ProSystem input[17] buffer
void Input_Poll(unsigned char input[17], bool leftDiffA, bool rightDiffA);

#endif // PROSYSTEM_SDL_INPUT_H
