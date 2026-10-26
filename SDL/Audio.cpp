#include "Audio.h"
#include "Tia.h"
#include "Pokey.h"
#include "Cartridge.h"

#include <SDL3/SDL.h>
#include <cstring>
#include <algorithm>

static SDL_AudioStream* g_audioStream = nullptr;
static int g_sourceFreq = 31440;
static int g_targetFreq = 44100;
static float g_volume = 1.0f;
static bool g_isMuted = false;

static bool RecreateAudioStream() {
    if (g_audioStream) {
        SDL_DestroyAudioStream(g_audioStream);
        g_audioStream = nullptr;
    }

    SDL_AudioSpec srcSpec;
    SDL_zero(srcSpec);
    srcSpec.format = SDL_AUDIO_U8;
    srcSpec.channels = 1;
    srcSpec.freq = g_sourceFreq;

    SDL_AudioSpec dstSpec;
    SDL_zero(dstSpec);
    dstSpec.format = SDL_AUDIO_U8;
    dstSpec.channels = 1;
    dstSpec.freq = g_targetFreq;

    g_audioStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &dstSpec,
        NULL,
        NULL
    );

    if (g_audioStream) {
        SDL_SetAudioStreamFormat(g_audioStream, &srcSpec, &dstSpec);
        SDL_ResumeAudioStreamDevice(g_audioStream);
        return true;
    }
    return false;
}

bool Audio_Init(int rawSourceFreq, int targetFreq) {
    g_sourceFreq = rawSourceFreq;
    g_targetFreq = targetFreq;
    return RecreateAudioStream();
}

void Audio_Shutdown() {
    if (g_audioStream) {
        SDL_DestroyAudioStream(g_audioStream);
        g_audioStream = nullptr;
    }
}

void Audio_Clear() {
    tia_Clear();
    pokey_Clear();
    if (g_audioStream) {
        SDL_ClearAudioStream(g_audioStream);
        uint8_t silence[AUDIO_SAMPLES_PER_FRAME];
        std::memset(silence, 0, sizeof(silence));
        SDL_PutAudioStreamData(g_audioStream, silence, sizeof(silence));
    }
}

void Audio_ProcessFrame() {
    if (!g_audioStream || g_isMuted) return;

    uint8_t frameAudio[AUDIO_SAMPLES_PER_FRAME];
    for (int i = 0; i < AUDIO_SAMPLES_PER_FRAME; ++i) {
        int16_t mixed = static_cast<int16_t>(tia_buffer[i]);
        if (cartridge_pokey) {
            mixed = (mixed + static_cast<int16_t>(pokey_buffer[i])) / 2;
        }
        mixed = static_cast<int16_t>(g_volume * mixed);
        frameAudio[i] = static_cast<uint8_t>(std::clamp<int16_t>(mixed, 0, 255));
    }

    if (SDL_GetAudioStreamQueued(g_audioStream) < (AUDIO_SAMPLES_PER_FRAME * 2)) {
        SDL_PutAudioStreamData(g_audioStream, frameAudio, sizeof(frameAudio));
    }
    SDL_PutAudioStreamData(g_audioStream, frameAudio, sizeof(frameAudio));
}

void Audio_SetVolume(float volume) {
    g_volume = std::clamp(volume, 0.0f, 1.0f);
}

float Audio_GetVolume() {
    return g_volume;
}

void Audio_SetMuted(bool muted) {
    g_isMuted = muted;
}

bool Audio_IsMuted() {
    return g_isMuted;
}

bool Audio_ToggleMute() {
    g_isMuted = !g_isMuted;
    return g_isMuted;
}

void Audio_SetTargetFrequency(int targetFreq) {
    if (g_targetFreq != targetFreq) {
        g_targetFreq = targetFreq;
        RecreateAudioStream();
    }
}

int Audio_GetTargetFrequency() {
    return g_targetFreq;
}

void Audio_SetSourceFrequency(int srcFreq) {
    if (g_sourceFreq != srcFreq) {
        g_sourceFreq = srcFreq;
        RecreateAudioStream();
    }
}
