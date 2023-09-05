#include "tetris_sound.h"

// http://soundfile.sapp.org/doc/WaveFormat/
#pragma pack(push, 1)
typedef struct wav_format {
    u8  chunkID[4];     // "RIFF"
    u32 chunkSize;
    u8  format[4];      // "WAVE"

    u8  subchunk1ID[4]; // "fmt "
    u32 subchunk1Size;
    u16 audioFormat;
    u16 numChannels;
    u32 sampleRate;
    u32 byteRate;
    u16 blockAlign;
    u16 bitsPerSample;

    u8  subchunk2ID[4]; // "data"
    u32 subchunk2Size;
    // Here comes the actual audio data
} wav_format;
#pragma pack(pop)

sound_buffer LoadWAV(const char* filePath) {
    i32 bytesRead;
    void* contents = EngineReadEntireFile(filePath, &bytesRead);
    if (bytesRead == 0) {
        return (sound_buffer){ 0 };
    }

    wav_format format = *(wav_format*)contents;

    if ((format.chunkID[0] != 'R' || format.chunkID[1] != 'I' || format.chunkID[2] != 'F' || format.chunkID[3] != 'F') || // "RIFF"
        (format.format[0] != 'W' || format.format[1] != 'A' || format.format[2] != 'V' || format.format[3] != 'E')     || // "WAVE"
        (format.audioFormat != 1)                                                                                      || // No compression
        (format.bitsPerSample != 16)                                                                                   || // Could probably solve this one
        (format.numChannels >= 3))
    {
        return (sound_buffer){ 0 };
    }

    sound_buffer result = { 
        .samples = (u8*)contents + 44, // 44 is the size of the header before the data
        .samplesCount = format.subchunk2Size / 2 // subchunk2Size is in bytes
    };

    if (format.sampleRate != SOUND_SAMPLES_PER_SECOND) {
        f32 ratio = format.sampleRate / (f32)SOUND_SAMPLES_PER_SECOND;

        result.samplesCount /= ratio;
        i16* buffer = EngineAllocate(result.samplesCount * 2);
        if (!buffer) {
            return (sound_buffer){ 0 };
        }

        for (i32 i = 0; i < result.samplesCount - 1; ++i) {
            f32 t = i * ratio;
            i32 index = (i32)t;
            t -= index;

            buffer[i] = (1.0f - t) * result.samples[index] + t * result.samples[index + format.numChannels];
        }

        result.samples = buffer;
        EngineFree(contents);
    }

    if (format.numChannels == 1) {
        i16* buffer = EngineAllocate(result.samplesCount * 4);
        if (!buffer) {
            return (sound_buffer){ 0 };
        }

        for (i32 i = 0; i < result.samplesCount; ++i) {
            buffer[2 * i]     = result.samples[i];
            buffer[2 * i + 1] = result.samples[i];
        }

        EngineFree(result.samples);
        result.samples = buffer;
        result.samplesCount *= 2;
    }


    return result;
}

i32 PlaySound(sound_buffer* audioBuffer, b32 isLooping, audio_channel* channels, i32 channelsCount) {
    for (i32 i = 0; i < channelsCount; ++i) {
        if (!channels[i].samples) {
            channels[i] = (audio_channel){
                .samples = audioBuffer->samples,
                .samplesCount = audioBuffer->samplesCount,
                .sampleIndex = 0,
                .isLooping = isLooping
            };
            return i;
        }
    }
}

void StopSound(i32 index, audio_channel* channels) {
    channels[index].samples = 0;
}

void StopAllSounds(audio_channel* channels, i32 channelCount) {
    for (i32 i = 0; i < channelCount; ++i) {
        channels[i].samples = 0;
    }
}

void ProcessSound(sound_buffer* soundBuffer, audio_channel* channels, i32 channelCount) {
    i16* samples = soundBuffer->samples;
    for (i32 i = 0; i < soundBuffer->samplesCount; ++i) {
        f32 sampleLeft  = 0.0f;
        f32 sampleRight = 0.0f;
        for (i32 j = 0; j < channelCount; ++j) {
            if (!channels[j].samples) {
                continue;
            }

            sampleLeft  += channels[j].samples[channels[j].sampleIndex++] / 32768.0f;
            sampleRight += channels[j].samples[channels[j].sampleIndex++] / 32768.0f;

            if (channels[j].sampleIndex >= channels[j].samplesCount) {
                if (channels[j].isLooping) {
                    channels[j].sampleIndex -= channels[j].samplesCount;
                }
                else {
                    channels[j].samples = 0;
                }
            }
        }

        *samples++ = Clamp(sampleLeft,  -1.0f, 1.0f) * 32767.0f;
        *samples++ = Clamp(sampleRight, -1.0f, 1.0f) * 32767.0f;
    }
}