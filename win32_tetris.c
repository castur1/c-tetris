#include <Windows.h>
#include <dsound.h>
#include "tetris.h"
#pragma comment(lib, "winmm.lib")  // Perhaps I should just add these to additional dependencies instead?
#pragma comment(lib, "dsound.lib") // ...Like, for compatability reasons and stuff

// Debug / testing
#include <stdio.h>
#include <math.h>


typedef struct win32_sound_buffer {
    i32 samplesPerSecond;
    i32 toneHz;
    i16 toneVolume;
    u32 runningSampleIndex;
    i32 wavePeriod;
    i32 bytesPerSample;
    i32 secondaryBufferSize;
    f32 tSine;
    DWORD latencySampleCount;
} win32_sound_buffer;

typedef struct win32_bitmap {
    BITMAPINFO info;
    void* memory;
    i32 width;
    i32 height;
    i32 pitch;
} win32_bitmap;

typedef struct ivec2 {
    i32 x;
    i32 y;
} ivec2;


static b32 g_isRunning;
static win32_bitmap g_bitmapBuffer;
static LPDIRECTSOUNDBUFFER g_secondarySoundBuffer; // Probably shouldn't be global


// Credit: Raymond Chen
static void ToggleFullscreen(HWND window) {
    static WINDOWPLACEMENT g_windowPosition = { sizeof(g_windowPosition) };
    DWORD style = GetWindowLong(window, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO monitorInfo = { sizeof(monitorInfo) };
        if (GetWindowPlacement(window, &g_windowPosition) && GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)) {
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, \
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, \
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &g_windowPosition);
        SetWindowPos(window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

static int InitDirectSound(HWND window, i32 samplesPerSecond, i32 secondaryBufferSize) {
    LPDIRECTSOUND directSound;

    if (FAILED(DirectSoundCreate(0, &directSound, 0))) {
        return 0;
    }

    if (FAILED(directSound->lpVtbl->SetCooperativeLevel(directSound, window, DSSCL_PRIORITY))) {
        return 0;
    }

    WAVEFORMATEX waveFormat = { 0 };
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 2;
    waveFormat.nSamplesPerSec = samplesPerSecond;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    // DSBCAPS_GLOBALFOCUS? Or even DSBCAPS_STICKYFOCUS?
    DSBUFFERDESC primaryBufferDescription = { .dwSize = sizeof(DSBUFFERDESC), .dwFlags = DSBCAPS_PRIMARYBUFFER };
    LPDIRECTSOUNDBUFFER primaryBuffer;
    if (FAILED(directSound->lpVtbl->CreateSoundBuffer(directSound, &primaryBufferDescription, &primaryBuffer, 0))) {
        return 0;
    }

    if (FAILED(primaryBuffer->lpVtbl->SetFormat(primaryBuffer, &waveFormat))) {
        return 0;
    }

    DSBUFFERDESC secondaryBufferDescription = {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = 0,
        .dwBufferBytes = secondaryBufferSize,
        .lpwfxFormat = &waveFormat
    };
    if (FAILED(directSound->lpVtbl->CreateSoundBuffer(directSound, &secondaryBufferDescription, &g_secondarySoundBuffer, 0))) {
        return 0;
    }
}

static void ClearSoundBuffer(win32_sound_buffer* soundBuffer) {
    VOID* region1;
    DWORD region1Size;
    VOID* region2;
    DWORD region2Size;

    if (SUCCEEDED(g_secondarySoundBuffer->lpVtbl->Lock(g_secondarySoundBuffer, 0, soundBuffer->secondaryBufferSize, &region1, &region1Size, &region2, &region2Size, 0))) {
        u8* byte = region1;
        for (DWORD byteIndex = 0; byteIndex < region1Size; ++byteIndex) {
            *byte++ = 0;
        }

        byte = region2;
        for (DWORD byteIndex = 0; byteIndex < region2Size; ++byteIndex) {
            *byte++ = 0;
        }
        Assert(region2Size == 0);
        
        g_secondarySoundBuffer->lpVtbl->Unlock(g_secondarySoundBuffer, region1, region1Size, region2, region2Size);
    }
}

static void FillSoundBuffer(win32_sound_buffer* destBuffer, TEST_sound_buffer* sourceBuffer, DWORD byteToLock, DWORD bytesToWrite) {
    VOID* region1;
    DWORD region1Size;
    VOID* region2;
    DWORD region2Size;

    if (SUCCEEDED(g_secondarySoundBuffer->lpVtbl->Lock(g_secondarySoundBuffer, byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
#if 1
        i16* sourceSample = sourceBuffer->samples;

        i16* destSample = region1;
        DWORD region1SampleCount = region1Size / destBuffer->bytesPerSample;
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex) {
            *destSample++ = *sourceSample;
            *destSample++ = *sourceSample++; // left and right are combined

            ++destBuffer->runningSampleIndex; // This has no reason to be here?
        }

        destSample = region2;
        DWORD region2SampleCount = region2Size / destBuffer->bytesPerSample;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex) {
            *destSample++ = *sourceSample;
            *destSample++ = *sourceSample++;

            ++destBuffer->runningSampleIndex;
        }
#else
        i16* sampleOut = region1;
        DWORD regionSampleCount = region1Size / destBuffer->bytesPerSample;
        for (DWORD sampleIndex = 0; sampleIndex < regionSampleCount; ++sampleIndex) {
            i16 sampleValue = destBuffer->toneVolume * sinf(destBuffer->tSine);
            *sampleOut++ = sampleValue;
            *sampleOut++ = sampleValue;

            ++destBuffer->runningSampleIndex;
            destBuffer->tSine += TWO_PI / destBuffer->wavePeriod;
            if (destBuffer->tSine > TWO_PI) {
                destBuffer->tSine -= TWO_PI;
            }
        }

        sampleOut = region2;
        regionSampleCount = region2Size / destBuffer->bytesPerSample;
        for (DWORD sampleIndex = 0; sampleIndex < regionSampleCount; ++sampleIndex) {
            i16 sampleValue = destBuffer->toneVolume * sinf(destBuffer->tSine);
            *sampleOut++ = sampleValue;
            *sampleOut++ = sampleValue;

            ++destBuffer->runningSampleIndex;
            destBuffer->tSine += TWO_PI / destBuffer->wavePeriod;
            if (destBuffer->tSine > TWO_PI) {
                destBuffer->tSine -= TWO_PI;
            }
        }
#endif
        g_secondarySoundBuffer->lpVtbl->Unlock(g_secondarySoundBuffer, region1, region1Size, region2, region2Size);
    }
}

static inline LARGE_INTEGER GetCurrentPerformanceCount(void) {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value;
}

static inline f32 PerformanceCountDiffInSeconds(LARGE_INTEGER startCount, LARGE_INTEGER endCount, LARGE_INTEGER performanceFrequency) {
    return (endCount.QuadPart - startCount.QuadPart) / (f32)performanceFrequency.QuadPart;
}

static ivec2 GetWindowDimensions(HWND window) {
    RECT clientRect;
    GetClientRect(window, &clientRect);
    return (ivec2){ clientRect.right - clientRect.left, clientRect.bottom - clientRect.top };
}

#define BYTES_PER_PIXEL 4
static void InitBitmap(win32_bitmap* bitmap, i32 width, i32 height) {
    if (bitmap->memory != NULL) {
        VirtualFree(bitmap->memory, 0, MEM_RELEASE);
    }

    bitmap->width  = width;
    bitmap->height = height;
    bitmap->pitch  = width * BYTES_PER_PIXEL;

    bitmap->info.bmiHeader = (BITMAPINFOHEADER){
        .biSize = sizeof(bitmap->info.bmiHeader),
        .biWidth = width,
        .biHeight = height,
        .biPlanes = 1,
        .biBitCount = BYTES_PER_PIXEL * 8,
        .biCompression = BI_RGB
    };

    i32 memorySize = width * height * BYTES_PER_PIXEL;
    bitmap->memory = VirtualAlloc(NULL, memorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}
#undef BYTES_PER_PIXEL

#define ADD_BARS 1
static void DisplayBitmapInWindow(const win32_bitmap* bitmap, HDC deviceContext, i32 windowWidth, i32 windowHeight) {
#if ADD_BARS
    f32 bitmapAspectRatio = (f32)bitmap->width / bitmap->height;

    if (windowHeight * bitmapAspectRatio < windowWidth) {
        i32 xOffset = (windowWidth - windowHeight * bitmapAspectRatio) / 2.0f + 1;

        PatBlt(deviceContext, 0, 0, xOffset, windowHeight, BLACKNESS);
        PatBlt(deviceContext, windowWidth - xOffset, 0, xOffset, windowHeight, BLACKNESS);

        SetStretchBltMode(deviceContext, STRETCH_DELETESCANS);
        StretchDIBits(deviceContext, xOffset, 0, windowHeight * bitmapAspectRatio, windowHeight, \
            0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
}
    else {
        i32 yOffset = (windowHeight - windowWidth / bitmapAspectRatio) / 2.0f + 1;

        PatBlt(deviceContext, 0, 0, windowWidth, yOffset, BLACKNESS);
        PatBlt(deviceContext, 0, windowHeight - yOffset, windowWidth, windowHeight, BLACKNESS);

        SetStretchBltMode(deviceContext, STRETCH_DELETESCANS);
        StretchDIBits(deviceContext, 0, yOffset, windowWidth, windowWidth / bitmapAspectRatio, \
            0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
    }
#else
    SetStretchBltMode(deviceContext, STRETCH_DELETESCANS);
    StretchDIBits(deviceContext, 0, 0, windowWidth, windowHeight, \
        0, 0, bitmap->width, bitmap->height, bitmap->memory, &bitmap->info, DIB_RGB_COLORS, SRCCOPY);
#endif
}
#undef ADD_BARS

static void UpdateKeyboardKey(keyboard_key_state* keyState, b32 isDown) {
    if (keyState->isDown != isDown) {
        keyState->isDown = isDown;
        keyState->didChangeState = true;
    }
}

ProcessPendingMessages(HWND window, win32_bitmap* bitmapBuffer, keyboard_state* keyboardState) {
    for (i32 i = 0; i < ArraySize(keyboardState->keys); ++i) {
        keyboardState->keys[i].didChangeState = false;
    }
    keyboardState->mouseLeft.didChangeState  = false;
    keyboardState->mouseRight.didChangeState = false;

    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_PAINT: {
                PAINTSTRUCT paint;
                HDC deviceContext = BeginPaint(window, &paint);
                ivec2 windowDimensions = GetWindowDimensions(window);
                DisplayBitmapInWindow(bitmapBuffer, deviceContext, windowDimensions.x, windowDimensions.y);
                EndPaint(window, &paint);
            } break;
            case WM_KEYDOWN:
            case WM_KEYUP: {
                b32 wasDown = (message.lParam & (1 << 30)) != 0;
                b32 isDown  = (message.lParam & (1 << 31)) == 0;

                b32 altKeyIsDown = false;
                if (isDown) {
                    altKeyIsDown = (message.lParam & (1 << 29)) != 0;
                }

                if (wasDown != isDown) {
                    switch (message.wParam) {
                        case VK_ESCAPE: {
                            g_isRunning = false;
                        } break;
                        case 'F': { // Should this really be here? Perhaps platform independence instead?
                            if (isDown) {
                                ToggleFullscreen(window);
                            }
                        } break;
                        // Is there a better solution?
                        case 'W': {
                            UpdateKeyboardKey(&keyboardState->w, isDown);
                        } break;
                        case 'A': {
                            UpdateKeyboardKey(&keyboardState->a, isDown);
                        } break;
                        case 'S': {
                            UpdateKeyboardKey(&keyboardState->s, isDown);
                        } break;
                        case 'D': {
                            UpdateKeyboardKey(&keyboardState->d, isDown);
                        } break;
                    }
                }
            } break;
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP: {
                UpdateKeyboardKey(&keyboardState->mouseLeft, message.wParam & MK_LBUTTON);
            } break;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP: {
                UpdateKeyboardKey(&keyboardState->mouseRight, message.wParam & MK_RBUTTON);
            } break;
        }

        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

static void GetMousePosition(HWND window, i32* x, i32* y) {
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(window, &mousePos);
    *x = mousePos.x;
    *y = mousePos.y;
}

static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_QUIT: {
            g_isRunning = false;
        } break;
        case WM_PAINT: { // Yes, this unfortunately needs to be here
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            ivec2 windowDimensions = GetWindowDimensions(window);
            DisplayBitmapInWindow(&g_bitmapBuffer, deviceContext, windowDimensions.x, windowDimensions.y);
            EndPaint(window, &paint);
        } break;
    }

    return DefWindowProc(window, message, wParam, lParam);
}

int CALLBACK WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prevInstance, _In_ LPSTR cmdLine, _In_ int showCmd) {
    WNDCLASSA windowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = instance,
        .lpszClassName = "tetris window class"
        // hIcon, hCursor
    };
    if (!RegisterClass(&windowClass)) {
        return 1;
    }

    HWND window = CreateWindowEx(0, windowClass.lpszClassName, L"Tetris", WS_OVERLAPPEDWINDOW | WS_VISIBLE, \
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, NULL);
    if (!window) {
        return 1;
    }

    HDC deviceContext = GetDC(window);

    win32_sound_buffer soundOutput = { 0 }; // I don't like this name
    soundOutput.samplesPerSecond = 44100;
    soundOutput.toneHz = 262;
    soundOutput.toneVolume = 4000;
    soundOutput.runningSampleIndex = 0;
    soundOutput.wavePeriod = soundOutput.samplesPerSecond / soundOutput.toneHz;
    soundOutput.bytesPerSample = sizeof(u16) * 2;
    soundOutput.secondaryBufferSize = 2.0f * soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
    soundOutput.tSine = 0.0f;
    soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 10;

    i16 soundSamples[88200]; // virtualAlloc? Also, 88200 is seconds * samplesPerSecond
    b32 soundIsValid = false;

    InitDirectSound(window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
    ClearSoundBuffer(&soundOutput);
    g_secondarySoundBuffer->lpVtbl->Play(g_secondarySoundBuffer, 0, 0, DSBPLAY_LOOPING);

    timeBeginPeriod(1);

    LARGE_INTEGER performanceFrequence;
    QueryPerformanceFrequency(&performanceFrequence);

    i32 refreshRate = 30;
    i32 screenRefreshRate = GetDeviceCaps(deviceContext, VREFRESH);
    if (screenRefreshRate > 1 && screenRefreshRate < refreshRate) { // This is probably unnecessary
        refreshRate = screenRefreshRate;
    }
    f32 secondsPerFrame = 1.0f / refreshRate;

    f32 secondsForLastFrame = secondsPerFrame;

    InitBitmap(&g_bitmapBuffer, 960, 540);

    keyboard_state keyboardState = { 0 };

    OnStartup();

    g_isRunning = true;
    while (g_isRunning) {
        LARGE_INTEGER performanceCountAtStartOfFrame = GetCurrentPerformanceCount();

        ProcessPendingMessages(window, &g_bitmapBuffer, &keyboardState);

        // Should I make this relative to the bitmap instead?
        GetMousePosition(window, &keyboardState.mouseX, &keyboardState.mouseY);

        bitmap graphicsBuffer = {
            .memory = g_bitmapBuffer.memory,
            .width  = g_bitmapBuffer.width,
            .height = g_bitmapBuffer.height,
            .pitch  = g_bitmapBuffer.pitch,
        };

        DWORD byteToLock = 0;
        DWORD bytesToWrite = 0;
        DWORD playCursor = 0;
        DWORD writeCursor = 0;
        if (g_secondarySoundBuffer->lpVtbl->GetCurrentPosition(g_secondarySoundBuffer, &playCursor, &writeCursor) == DS_OK) {
            if (!soundIsValid) {
                soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
            }
            byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;

            DWORD targetCursor = (playCursor + soundOutput.latencySampleCount * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
            if (byteToLock >= targetCursor) {
                bytesToWrite = soundOutput.secondaryBufferSize - byteToLock + targetCursor;
            }
            else {
                bytesToWrite = targetCursor - byteToLock;
            }

            soundIsValid = true;
        }
        else {
            soundIsValid = false;
        }

        TEST_sound_buffer soundBuffer = { 0 };
        soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
        soundBuffer.samples = soundSamples;
        soundBuffer.samplesCount = bytesToWrite / soundOutput.bytesPerSample;

        Update(&graphicsBuffer, &soundBuffer, &keyboardState, secondsForLastFrame);

        if (soundIsValid) {
            FillSoundBuffer(&soundOutput, &soundBuffer, byteToLock, bytesToWrite);
        }

        ivec2 windowDimensions = GetWindowDimensions(window);
        DisplayBitmapInWindow(&g_bitmapBuffer, deviceContext, windowDimensions.x, windowDimensions.y);

        LARGE_INTEGER performanceCountAtEndOfFrame = GetCurrentPerformanceCount();
        f32 secondsElapsedForFrame = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, performanceCountAtEndOfFrame, performanceFrequence);
        if (secondsElapsedForFrame < secondsPerFrame) {
            secondsForLastFrame = secondsPerFrame;
            f32 millisecondsToSleep = 1000 * (secondsPerFrame - secondsElapsedForFrame);
            if (millisecondsToSleep >= 1) {
                Sleep(millisecondsToSleep - 1);
            }
            while (secondsElapsedForFrame < secondsPerFrame) {
                secondsElapsedForFrame = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, GetCurrentPerformanceCount(), performanceFrequence);
            }
        }
        else {
            secondsForLastFrame = secondsElapsedForFrame;
        }

#if 1
        f32 debugSeconds = PerformanceCountDiffInSeconds(performanceCountAtStartOfFrame, GetCurrentPerformanceCount(), performanceFrequence);
        f32 debugFPS = 1.0f / debugSeconds;
        char debugBuffer[64];
        sprintf_s(debugBuffer, 64, "%.2f ms/f, %.2f fps\n", 1000.0f * debugSeconds, debugFPS);
        OutputDebugStringA(debugBuffer);
#endif
    }

    timeEndPeriod(1);

    return 0;
}