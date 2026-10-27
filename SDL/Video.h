#ifndef PROSYSTEM_SDL_VIDEO_H
#define PROSYSTEM_SDL_VIDEO_H

#include <SDL3/SDL.h>
#include <string>

constexpr int VISIBLE_WIDTH = 320;
constexpr int VISIBLE_HEIGHT = 223;

// Lifecycle
bool Video_Init(const std::string& title, int windowWidth = 640, int windowHeight = 446);
void Video_Shutdown();

// Frame Presentation
void Video_ClearScreen();
void Video_RenderFrame();
void Video_Present();

// Render placeholder or splash text when emulation is stopped
void Video_RenderText(float x, float y, const char* text);

// Viewport / Window modes
void Video_ToggleFullscreen();
bool Video_IsFullscreen();

void Video_ToggleAspectRatio();
bool Video_IsAspectPreserved();

// Screenshot
void Video_SaveScreenshot();

// Accessors for subsystems that need window/renderer handles (Dialogs, Menu, etc.)
SDL_Window*   Video_GetWindow();
SDL_Renderer* Video_GetRenderer();

#endif // PROSYSTEM_SDL_VIDEO_H
