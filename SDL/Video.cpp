#include "Video.h"
#include "Maria.h"
#include "Palette.h"
#include "xbrz.h"

#include <iostream>
#include <filesystem>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace fs = std::filesystem;

static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_texture = nullptr;

static bool g_isFullscreen = false;
static bool g_preserveAspect = true;
static bool g_xbrzEnabled = false;
// The visible area is 223 lines for NTSC and 272 for PAL; buffers are sized for the larger
static constexpr int MAX_VISIBLE_HEIGHT = 272;
static int g_height = VISIBLE_HEIGHT;
static std::vector<uint32_t> g_scaledBuffer(VISIBLE_WIDTH * 2 * MAX_VISIBLE_HEIGHT * 2);
static uint32_t g_pixelBuffer[VISIBLE_WIDTH * MAX_VISIBLE_HEIGHT];

static void Video_CreateTexture() {
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = nullptr;
    }

    int scale = g_xbrzEnabled ? 2 : 1;
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  VISIBLE_WIDTH * scale, g_height * scale);
    if (g_texture) {
        SDL_SetTextureScaleMode(g_texture, SDL_SCALEMODE_NEAREST);
    }
}

static void Video_SetLogicalSize() {
    SDL_SetRenderLogicalPresentation(
        g_renderer, VISIBLE_WIDTH, g_height,
        g_preserveAspect ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_STRETCH
    );
}

bool Video_Init(const std::string& title, int windowWidth, int windowHeight) {
    g_window = SDL_CreateWindow(title.c_str(), windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, NULL);
    if (!g_renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
        return false;
    }

    SDL_SetRenderVSync(g_renderer, 1);
    Video_SetLogicalSize();

    Video_CreateTexture();
    if (!g_texture) {
        std::cerr << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void Video_Shutdown() {
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = nullptr;
    }
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }
}

void Video_ClearScreen() {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
}

void Video_RenderFrame() {
    if (!g_renderer || !g_texture) return;

    int height = static_cast<int>(maria_visibleArea.GetHeight());
    if (height < 1) height = 1;
    if (height > MAX_VISIBLE_HEIGHT) height = MAX_VISIBLE_HEIGHT;
    if (height != g_height) {
        g_height = height;
        Video_SetLogicalSize();
        Video_CreateTexture();
        if (!g_texture) return;
    }

    const int visibleOffset = (maria_visibleArea.top - maria_displayArea.top) * VISIBLE_WIDTH;

    for (int i = 0; i < VISIBLE_WIDTH * g_height; ++i) {
        uint8_t colorIdx = maria_surface[visibleOffset + i];
        uint8_t r = palette_data[colorIdx * 3 + 0];
        uint8_t g = palette_data[colorIdx * 3 + 1];
        uint8_t b = palette_data[colorIdx * 3 + 2];
        g_pixelBuffer[i] = (0xFF000000) | (r << 16) | (g << 8) | b;
    }

    if (g_xbrzEnabled) {
        xbrz::scale(2, g_pixelBuffer, g_scaledBuffer.data(), VISIBLE_WIDTH, g_height, xbrz::ColorFormat::ARGB);
        SDL_UpdateTexture(g_texture, NULL, g_scaledBuffer.data(), (VISIBLE_WIDTH * 2) * sizeof(uint32_t));
    } else {
        SDL_UpdateTexture(g_texture, NULL, g_pixelBuffer, VISIBLE_WIDTH * sizeof(uint32_t));
    }

    SDL_RenderTexture(g_renderer, g_texture, NULL, NULL);
}

void Video_Present() {
    if (g_renderer) {
        SDL_RenderPresent(g_renderer);
    }
}

void Video_RenderText(float x, float y, const char* text) {
    if (!g_renderer || !text) return;
    SDL_SetRenderDrawColor(g_renderer, 200, 200, 200, 255);
    SDL_RenderDebugText(g_renderer, x, y, text);
}

void Video_ToggleFullscreen() {
    if (!g_window) return;
    g_isFullscreen = !g_isFullscreen;
    SDL_SetWindowFullscreen(g_window, g_isFullscreen);
}

bool Video_IsFullscreen() {
    return g_isFullscreen;
}

void Video_ToggleAspectRatio() {
    if (!g_renderer) return;
    g_preserveAspect = !g_preserveAspect;
    Video_SetLogicalSize();
}

bool Video_IsAspectPreserved() {
    return g_preserveAspect;
}

void Video_SaveScreenshot() {
    if (!g_renderer) return;

    SDL_Surface* surface = SDL_RenderReadPixels(g_renderer, NULL);
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

void Video_ToggleFilter() {
    if (!g_renderer) return;

    g_xbrzEnabled = !g_xbrzEnabled;

    // Recreate streaming texture to match native 1x or 2x
    Video_CreateTexture();
}

void Video_SetFilter(bool enabled) {
    if (g_xbrzEnabled == enabled) return;
    Video_ToggleFilter();
}

bool Video_IsFilterEnabled() {
    return g_xbrzEnabled;
}

SDL_Window* Video_GetWindow() {
    return g_window;
}

SDL_Renderer* Video_GetRenderer() {
    return g_renderer;
}
