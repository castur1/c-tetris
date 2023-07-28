#include "tetris.h"

void OnStartup(void) {

}

void Update(bitmap* graphicsBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    static i32 xOffset = 0;
    static i32 yOffset = 0;

    xOffset += 5 * (keyboardState->d.isDown - keyboardState->a.isDown);
    yOffset += 5 * (keyboardState->w.isDown - keyboardState->s.isDown);  

    if (xOffset < 0) {
        xOffset = 0;
    }
    if (yOffset < 0) {
        yOffset = 0;
    }

    u32* pixel = graphicsBuffer->memory;
    for (i32 y = 0; y < graphicsBuffer->height; ++y) {
        for (i32 x = 0; x < graphicsBuffer->width; ++x) {
            *pixel++ = RGB(x + xOffset, 0, y + yOffset);
        }
    }
}

void GetSoundSamples(sound_buffer* soundBuffer, keyboard_state* TEST_keyboardState) {
    static f32 tSine = 0.0f;
    i16 toneVolume = 4000;
    i32 toneHz = TEST_keyboardState->w.isDown ? 523 : 262;
    i32 wavePeriod = soundBuffer->samplesPerSecond / toneHz;

    i16* samples = soundBuffer->samples;
    for (i32 sampleIndex = 0; sampleIndex < soundBuffer->samplesCount; ++sampleIndex) {
        i16 sampleValue = toneVolume * sinf(tSine);
        *samples++ = sampleValue;

        tSine += TWO_PI / wavePeriod;
        if (tSine > TWO_PI) {
            tSine -= TWO_PI;
        }
    }
}