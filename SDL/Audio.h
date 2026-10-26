#ifndef PROSYSTEM_SDL_AUDIO_H
#define PROSYSTEM_SDL_AUDIO_H

#include <cstdint>

constexpr int AUDIO_SAMPLES_PER_FRAME = 524;

// Lifecycle
bool Audio_Init(int rawSourceFreq = 31440, int targetFreq = 44100);
void Audio_Shutdown();
void Audio_Clear();

// Frame processing
void Audio_ProcessFrame();

// Controls & Settings
void Audio_SetVolume(float volume);
float Audio_GetVolume();

void Audio_SetMuted(bool muted);
bool Audio_IsMuted();
bool Audio_ToggleMute();

void Audio_SetTargetFrequency(int targetFreq);
int  Audio_GetTargetFrequency();

void Audio_SetSourceFrequency(int srcFreq);

#endif // PROSYSTEM_SDL_AUDIO_H
