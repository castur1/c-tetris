#include "tetris.h"
#include "tetris_graphics.h"
#include "tetris_sound.h"

#define AUDIO_CHANNEL_COUNT 16

static void TEST_renderBackround(bitmap_buffer* graphicsBuffer, i32 xOffset, i32 yOffset) {
    u32* pixel = graphicsBuffer->memory;
    for (i32 y = 0; y < graphicsBuffer->height; ++y) {
        for (i32 x = 0; x < graphicsBuffer->width; ++x) {
            *pixel++ = RGBToU32(x + xOffset, 0, y + yOffset);
        }
    }
}

typedef enum tetromino_type {
    tetromino_type_empty,
    tetromino_type_I,
    tetromino_type_O,
    tetromino_type_T,
    tetromino_type_S,
    tetromino_type_Z,
    tetromino_type_J,
    tetromino_type_L,
} tetromino_type;

// Tetromino struct? Combining type and rotation
// Board struct? Tiles, width, height, tile size, x, y, etc. Maybe not xy and/or tile size?

static const u16 tetromino_empty[4] = { 0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000 };
static const u16 tetromino_I[4]     = { 0b0000111100000000, 0b0100010001000100, 0b0000000011110000, 0b0010001000100010 };
static const u16 tetromino_O[4]     = { 0b0000011001100000, 0b0000011001100000, 0b0000011001100000, 0b0000011001100000 };
static const u16 tetromino_T[4]     = { 0b0010011100000000, 0b0010011000100000, 0b0000011100100000, 0b0010001100100000 };
static const u16 tetromino_S[4]     = { 0b0110001100000000, 0b0010011001000000, 0b0000011000110000, 0b0001001100100000 };
static const u16 tetromino_Z[4]     = { 0b0011011000000000, 0b0100011000100000, 0b0000001101100000, 0b0010001100010000 };
static const u16 tetromino_J[4]     = { 0b0001011100000000, 0b0110001000100000, 0b0000011101000000, 0b0010001000110000 };
static const u16 tetromino_L[4]     = { 0b0100011100000000, 0b0010001001100000, 0b0000011100010000, 0b0011001000100000 };

static const u16* tetrominoes[8] = {
    tetromino_empty,
    tetromino_I,
    tetromino_O,
    tetromino_T,
    tetromino_S,
    tetromino_Z,
    tetromino_J,
    tetromino_L
};

static u32 tetrominoColours[] = {
    RGBToU32(0, 0, 0),     // Empty
    RGBToU32(0, 255, 255), // I
    RGBToU32(255, 255, 0), // O
    RGBToU32(255, 0, 255), // T
    RGBToU32(0, 255, 0),   // S
    RGBToU32(255, 0, 0),   // Z
    RGBToU32(0, 0, 255),   // J
    RGBToU32(255, 128, 0), // L
};

static void TEST_PlaceTetromino(tetromino_type* board, i32 boardWidth, i32 x, i32 y, tetromino_type type, i32 rotation) {
    u16 tetromino = tetrominoes[type][rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (tetromino & (1 << i)) {
            i32 xi = x + i % 4;
            i32 yi = y + i / 4;
            board[yi * boardWidth + xi] = type;
        }
    }
}

static void TEST_DrawTetromino(bitmap_buffer* graphicsBuffer, i32 x, i32 y, i32 size, tetromino_type type, i32 rotation) {
    u16 tetromino = tetrominoes[type][rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (tetromino & (1 << i)) {
            i32 xOffset = (i % 4) * size;
            i32 yOffset = (i / 4) * size;
            DrawRectangle(graphicsBuffer, x + xOffset, y + yOffset, size, size, tetrominoColours[type]);
        }
    }
}

static void TEST_DrawBoard(bitmap_buffer* graphicsBuffer, tetromino_type* board, i32 boardWidth, i32 boardHeight, i32 tileSize, i32 xPos, i32 yPos) {
    for (i32 y = 0; y < boardHeight; ++y) {
        for (i32 x = 0; x < boardWidth; ++x) {
            tetromino_type tile = board[y * boardWidth + x];
            DrawRectangle(graphicsBuffer, xPos + x * tileSize, yPos + y * tileSize, tileSize, tileSize, tetrominoColours[tile]);
        }
    }
}

#define BOARD_WIDTH  10
#define BOARD_HEIGHT 22

typedef struct game_state {
    i32 xOffset;
    i32 yOffset;

    tetromino_type board[BOARD_HEIGHT][BOARD_WIDTH];

    audio_channel audioChannels[AUDIO_CHANNEL_COUNT];

    bitmap_buffer testBitmap1;
    bitmap_buffer testBitmap2;
    audio_buffer testWAVData1;
    audio_buffer testWAVData2;
} game_state;

static game_state g_gameState;

void OnStartup(void) {
    g_gameState.testBitmap1  = LoadBMP("assets/OpacityTest.bmp");
    g_gameState.testBitmap2  = LoadBMP("assets/opacity_test.bmp");
    g_gameState.testWAVData1 = LoadWAV("assets/wav_test3.wav");
    g_gameState.testWAVData2 = LoadWAV("assets/explosion.wav");

    PlaySound(&g_gameState.testWAVData1, true, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);

    TEST_PlaceTetromino(g_gameState.board, BOARD_WIDTH, 0, -2, tetromino_type_S, 0);
    TEST_PlaceTetromino(g_gameState.board, BOARD_WIDTH, 2, -2, tetromino_type_T, 0);
}

// Is there really a need for sound_buffer? Doesn't audio_buffer suffice?
void Update(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    f32 scrollSpeed = 256.0f;
    g_gameState.xOffset += (keyboardState->d.isDown - keyboardState->a.isDown) * scrollSpeed * deltaTime;
    g_gameState.yOffset += (keyboardState->w.isDown - keyboardState->s.isDown) * scrollSpeed * deltaTime;  
    g_gameState.xOffset = Max(0, g_gameState.xOffset);
    g_gameState.yOffset = Max(0, g_gameState.yOffset);
    
    TEST_renderBackround(graphicsBuffer, g_gameState.xOffset, g_gameState.yOffset);

#if 1
    i32 tileSize = graphicsBuffer->height / (f32)(BOARD_HEIGHT - 1.5f);
    i32 left = graphicsBuffer->width / 2 - BOARD_WIDTH * tileSize / 2;
    TEST_DrawBoard(graphicsBuffer, g_gameState.board, BOARD_WIDTH, BOARD_HEIGHT, tileSize, left, 0);

    i32 x = 3;
    i32 y = 7;
    static tetromino_type type = tetromino_type_L;
    type = (type + (keyboardState->d.isDown && keyboardState->d.didChangeState) - (keyboardState->a.isDown && keyboardState->a.didChangeState) + 8) % 8;
    static i32 rotation = 0;
    rotation = (rotation + (keyboardState->w.isDown && keyboardState->w.didChangeState) - (keyboardState->s.isDown && keyboardState->s.didChangeState) + 4) % 4;
    TEST_DrawTetromino(graphicsBuffer, left + x * tileSize, y * tileSize, tileSize, type, rotation);
#endif

    DrawRectangle(graphicsBuffer, keyboardState->mouseX, keyboardState->mouseY, 16, 16, 0xFFFFFF);

    DrawBitmap(graphicsBuffer, &g_gameState.testBitmap1, 50, 50);
    DrawBitmap(graphicsBuffer, &g_gameState.testBitmap2, g_gameState.testBitmap1.width + 50, 50);

    if (keyboardState->a.isDown && keyboardState->a.didChangeState) {
        PlaySound(&g_gameState.testWAVData2, false, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
    }

    if (keyboardState->s.isDown && keyboardState->s.didChangeState) {
        StopSound(0, g_gameState.audioChannels);
    }


    ProcessSound(soundBuffer, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
}