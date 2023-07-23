#include "tetris.h"

void OnStartup(void) {

}

#define RGB(r, g, b) ((r) << 16) | ((g) << 8) | (b)
void Update(bitmap* graphicsBuffer, TEST_sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    // Graphics

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

    // Sound

    static f32 tSine = 0.0f;
    i16 toneVolume = 4000;
    i32 toneHz = 262;
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