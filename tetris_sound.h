#ifndef TETRIS_SOUND_H
#define TETRIS_SOUND_H

#include "tetris.h"


typedef struct audio_buffer {
    i32 sampleCount;
    i16* samples;
} audio_buffer;

typedef struct audio_channel {
    i16* samples;
    i32 samplesCount;
    i32 sampleIndex;
    b32 isLooping;
} audio_channel;


extern audio_buffer LoadWAV(const char* filePath);
extern i32 PlaySound(audio_buffer* audioBuffer, b32 isLooping, audio_channel* channels, i32 channelsCount);
extern void StopSound(i32 index, audio_channel* channels);
extern void ProcessSound(sound_buffer* soundBuffer, audio_channel* channels, i32 channelCount);

#endif