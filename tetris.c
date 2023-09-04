#include "tetris.h"
#include "tetris_graphics.h"
#include "tetris_sound.h"
#include "tetris_random.h"


#define AUDIO_CHANNEL_COUNT 16

#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20

#define AUTO_MOVE_DELAY 0.2f
#define AUTO_MOVE_SPEED 0.05f
#define SOFT_DROP_SPEED 0.033f

#define PRESSED(key) (key.isDown && key.didChangeState)


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

static const u16 TETROMINO_EMPTY[4] = { 0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000 };
static const u16 TETROMINO_I[4]     = { 0b0000111100000000, 0b0100010001000100, 0b0000000011110000, 0b0010001000100010 };
static const u16 TETROMINO_O[4]     = { 0b0000011001100000, 0b0000011001100000, 0b0000011001100000, 0b0000011001100000 };
static const u16 TETROMINO_T[4]     = { 0b0010011100000000, 0b0010011000100000, 0b0000011100100000, 0b0010001100100000 };
static const u16 TETROMINO_S[4]     = { 0b0110001100000000, 0b0010011001000000, 0b0000011000110000, 0b0001001100100000 };
static const u16 TETROMINO_Z[4]     = { 0b0011011000000000, 0b0100011000100000, 0b0000001101100000, 0b0010001100010000 };
static const u16 TETROMINO_J[4]     = { 0b0001011100000000, 0b0110001000100000, 0b0000011101000000, 0b0010001000110000 };
static const u16 TETROMINO_L[4]     = { 0b0100011100000000, 0b0010001001100000, 0b0000011100010000, 0b0011001000100000 };

static const u16* TETROMINOES[8] = {
    TETROMINO_EMPTY,
    TETROMINO_I,
    TETROMINO_O,
    TETROMINO_T,
    TETROMINO_S,
    TETROMINO_Z,
    TETROMINO_J,
    TETROMINO_L
};

typedef struct board_t {
    tetromino_type* tiles;
    i32 width;
    i32 height;
    i32 size;
    i32 x;
    i32 y;
    i32 tileSize;
    i32 widthPx;
    i32 heightPx;
} board_t;

static board_t InitBoard(i32 width, i32 height, i32 x, i32 y, i32 tileSize) {
    return (board_t) {
        .tiles    = EngineAllocate(width * height * sizeof(tetromino_type)),
        .width    = width,
        .height   = height,
        .x        = x,
        .y        = y,
        .tileSize = tileSize,
        .size     = width * height,
        .widthPx  = width * tileSize,
        .heightPx = height * tileSize
    };
}

static void SetBoardTileSize(board_t* board, i32 tileSize) {
    board->tileSize = tileSize;
    board->widthPx  = board->width  * tileSize;
    board->heightPx = board->height * tileSize;
}

static void ClearBoard(board_t* board) {
    for (i32 i = 0; i < board->size; ++i) {
        board->tiles[i] = tetromino_type_empty;
    }
}

typedef struct tetromino_t {
    tetromino_type type;
    i32 rotation;
    i32 x;
    i32 y;
} tetromino_t;

static tetromino_t InitTetromino(tetromino_type type, i32 rotation, i32 x, i32 y) {
    return (tetromino_t){ .type = type, .rotation = rotation, .x = x, .y = y };
}

static void PlaceTetromino(board_t* board, tetromino_t* tetromino) {
    u16 bitField = TETROMINOES[tetromino->type][tetromino->rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (bitField & (1 << i)) {
            i32 x = tetromino->x + i % 4;
            i32 y = tetromino->y + i / 4;
            board->tiles[y * board->width + x] = tetromino->type;
        }
    }
}

static void DrawTetrominoInScreen(bitmap_buffer* graphicsBuffer, tetromino_t* tetromino, i32 size, bitmap_buffer* sprite, i32 opacity) {
    u16 bitField = TETROMINOES[tetromino->type][tetromino->rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (bitField & (1 << i)) {
            i32 xOffset = (i % 4) * size;
            i32 yOffset = (i / 4) * size;
            DrawBitmap(graphicsBuffer, sprite, tetromino->x + xOffset, tetromino->y + yOffset, size, opacity);
        }
    }
}

static void DrawTetrominoInBoard(bitmap_buffer* graphicsBuffer, board_t* board, tetromino_t* tetromino, bitmap_buffer* sprite, i32 opacity) {
    u16 bitField = TETROMINOES[tetromino->type][tetromino->rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (bitField & (1 << i)) {
            i32 x = tetromino->x + (i % 4);
            i32 y = tetromino->y + (i / 4);
            DrawBitmap(graphicsBuffer, sprite, board->x + x * board->tileSize, board->y + y * board->tileSize, board->tileSize, opacity);
        }
    }
}

static void DrawBoard(bitmap_buffer* graphicsBuffer, board_t* board, bitmap_buffer* sprites) {
    for (i32 y = 0; y < board->height; ++y) {
        for (i32 x = 0; x < board->width; ++x) {
            tetromino_type tile = board->tiles[y * board->width + x];
            if (tile != tetromino_type_empty) {
                DrawBitmap(graphicsBuffer, &sprites[tile], board->x + x * board->tileSize, board->y + y * board->tileSize, board->tileSize, OPACITY_NONE);
            }
        }
    }
}

static b32 IsTetrominoPosValid(board_t* board, tetromino_t* tetromino) {
    u16 bitField = TETROMINOES[tetromino->type][tetromino->rotation];
    for (i32 i = 0; i < 16; ++i) { 
        if (bitField & (1 << i)) {
            i32 x = tetromino->x + (i % 4);
            i32 y = tetromino->y + (i / 4);
            if (x < 0 || x >= board->width || y < 0 || board->tiles[y * board->width + x] != tetromino_type_empty) {
                return false;
            }
        }
    }

    return true;
}

static b32 ProcessLineClears(board_t* board, tetromino_t* tetromino) {
    i32 lineClearCount = 0;

    i32 y = tetromino->y + 3;
    while (y >= 0 && y >= tetromino->y) {
        b32 isLineClear = true;
        for (i32 x = 0; x < board->width; ++x) {
            if (board->tiles[y * board->width + x] == tetromino_type_empty) {
                isLineClear = false;
                break;
            }
        }

        if (isLineClear) {
            ++lineClearCount;
            for (i32 i = y; i < board->height - 1; ++i) {
                for (i32 j = 0; j < board->width; ++j) {
                    board->tiles[i * board->width + j] = board->tiles[(i + 1) * board->width + j];
                }
            }
        }

        --y;
    }

    return lineClearCount;
}

static void RandomizeBag(tetromino_type* bag) {
    bag[0] = bag[7];
    for (i32 i = 1; i < 7;) {
        i32 attempt = RandomI32InRange(1, 7);
        for (i32 j = 0; j < i; ++j) {
            if (bag[j] == attempt) {
                attempt = 0;
                break;
            }
        }
        if (attempt == 0) {
            continue;
        }
        bag[i++] = attempt;
    }
    bag[7] = RandomI32InRange(1, 7);
}

static tetromino_type GetNextTetrominoFromBag(tetromino_type* bag, i32* bagIndex) {
    tetromino_type result = bag[(*bagIndex)++];

    if (*bagIndex >= 7) {
        *bagIndex = 0;
        RandomizeBag(bag, 7);
    }

    return result;
}

typedef struct game_state {
    f32 timerFallSpeed;
    f32 timerAutoMoveDelay;
    f32 timerAutoMoveSpeed;

    board_t board;
    tetromino_t current;
    tetromino_t next;
    tetromino_t hold;
    b32 didUseHoldBox;
    tetromino_type bag[8];
    i32 bagIndex;
    i32 score;
    i32 level;

    audio_channel audioChannels[AUDIO_CHANNEL_COUNT];
} game_state;

typedef struct game_data {
    bitmap_buffer tetrominoes[8];

    bitmap_buffer background;

    sound_buffer testWAVData1;
    sound_buffer testWAVData2;
} game_data;

static game_state g_gameState;
static game_data g_gameData;

void OnStartup(void) {
    g_gameData.tetrominoes[1] = LoadBMP("assets/tetromino_cyan.bmp");
    g_gameData.tetrominoes[2] = LoadBMP("assets/tetromino_yellow.bmp");
    g_gameData.tetrominoes[3] = LoadBMP("assets/tetromino_purple.bmp");
    g_gameData.tetrominoes[4] = LoadBMP("assets/tetromino_green.bmp");
    g_gameData.tetrominoes[5] = LoadBMP("assets/tetromino_red.bmp");
    g_gameData.tetrominoes[6] = LoadBMP("assets/tetromino_blue.bmp");
    g_gameData.tetrominoes[7] = LoadBMP("assets/tetromino_orange.bmp");

    g_gameData.background = LoadBMP("assets/background.bmp");

    g_gameData.testWAVData1 = LoadWAV("assets/wav_test3.wav");
    g_gameData.testWAVData2 = LoadWAV("assets/explosion.wav");

    PlaySound(&g_gameData.testWAVData1, true, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);

    RandomInit();

    g_gameState.board = InitBoard(BOARD_WIDTH, BOARD_HEIGHT, 470, 20, 34);

    g_gameState.bag[7] = RandomI32InRange(1, 7);
    RandomizeBag(g_gameState.bag);

    g_gameState.current = InitTetromino(GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex), 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4);
    g_gameState.next = InitTetromino(GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex), 0, 838, 564);
    g_gameState.hold = InitTetromino(tetromino_type_empty, 0, 838, 400);

    g_gameState.score = 0;
    g_gameState.level = 1;
}

void Update(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    if (keyboardState->right.isDown) {
        g_gameState.timerAutoMoveDelay += deltaTime;
        if (g_gameState.timerAutoMoveDelay >= AUTO_MOVE_DELAY) {
            g_gameState.timerAutoMoveSpeed += deltaTime;
        }
        if (g_gameState.timerAutoMoveSpeed >= AUTO_MOVE_SPEED || keyboardState->right.didChangeState) {
            g_gameState.timerAutoMoveSpeed = 0.0f;
            ++g_gameState.current.x;
            if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                --g_gameState.current.x;
            }
        }
    }
    else if (keyboardState->left.isDown) {
        g_gameState.timerAutoMoveDelay += deltaTime;
        if (g_gameState.timerAutoMoveDelay >= AUTO_MOVE_DELAY) {
            g_gameState.timerAutoMoveSpeed += deltaTime;
        }
        if (g_gameState.timerAutoMoveSpeed >= AUTO_MOVE_SPEED || keyboardState->left.didChangeState) {
            g_gameState.timerAutoMoveSpeed = 0.0f;
            --g_gameState.current.x;
            if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                ++g_gameState.current.x;
            }
        }
    }
    else {
        g_gameState.timerAutoMoveDelay = 0.0f;
    }

    if (PRESSED(keyboardState->x)) {
        g_gameState.current.rotation = (g_gameState.current.rotation + 5) % 4;
        if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
            ++g_gameState.current.x;
            if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                g_gameState.current.x -= 2;
                if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                    g_gameState.current.rotation = (g_gameState.current.rotation + 3) % 4;
                }
            }
        }
    }
    else if (PRESSED(keyboardState->z)) {
        g_gameState.current.rotation = (g_gameState.current.rotation + 3) % 4;
        if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
            ++g_gameState.current.x;
            if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                g_gameState.current.x -= 2;
                if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                    g_gameState.current.rotation = (g_gameState.current.rotation + 5) % 4;
                }
            }
        }
    }

    if (PRESSED(keyboardState->c) && !g_gameState.didUseHoldBox) {
        g_gameState.didUseHoldBox = true;

        tetromino_type currentType = g_gameState.current.type;
        if (g_gameState.hold.type == tetromino_type_empty) {
            g_gameState.current = InitTetromino(g_gameState.next.type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_gameData.tetrominoes);
            g_gameState.next.type = GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex);
        }
        else {
            g_gameState.current = InitTetromino(g_gameState.hold.type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_gameData.tetrominoes);
        }
        g_gameState.hold.type = currentType;
    }

    b32 didHardDrop = false;
    if (keyboardState->up.isDown && keyboardState->up.didChangeState) {
        didHardDrop = true;
        while (IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
            --g_gameState.current.y;
        }
        ++g_gameState.current.y;
    }

    f32 gravityInSeconds;
    if (keyboardState->down.isDown) {
        gravityInSeconds = 0.033f;
    }
    else {
        gravityInSeconds = 1.0f;
        f32 base = 0.8f - (g_gameState.level - 1) * 0.007f;
        for (i32 i = 0; i < g_gameState.level - 1; ++i) {
            gravityInSeconds *= base;
        }
    }

    g_gameState.timerFallSpeed += deltaTime;
    if (g_gameState.timerFallSpeed >= gravityInSeconds || didHardDrop) {
        g_gameState.timerFallSpeed = 0.0f;

        --g_gameState.current.y;

        if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
            ++g_gameState.current.y;
            PlaceTetromino(&g_gameState.board, &g_gameState.current);

            i32 lineClearCount = ProcessLineClears(&g_gameState.board, &g_gameState.current);
            switch (lineClearCount) {
                case 1: {
                    g_gameState.score += 1;
                } break;
                case 2: {
                    g_gameState.score += 3;
                } break;
                case 3: {
                    g_gameState.score += 5;
                } break;
                case 4: {
                    g_gameState.score += 8;
                } break;
            }

            if (g_gameState.score >= g_gameState.level * 5) {
                ++g_gameState.level;
                PlaySound(&g_gameData.testWAVData2, false, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
            }

            g_gameState.current = InitTetromino(g_gameState.next.type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_gameData.tetrominoes);
            g_gameState.next.type = GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex);

            if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                ClearBoard(&g_gameState.board);
                RandomizeBag(g_gameState.bag);
                g_gameState.hold.type = tetromino_type_empty;
                g_gameState.score = 0;
                g_gameState.level = 1;
            }

            g_gameState.didUseHoldBox = false;
        }
    }

    tetromino_t ghost = g_gameState.current;
    while (IsTetrominoPosValid(&g_gameState.board, &ghost)) {
        --ghost.y;
    }
    ++ghost.y;

    DrawBitmap(graphicsBuffer, &g_gameData.background, 0, 0, 1280, OPACITY_NONE);

    DrawBoard(graphicsBuffer, &g_gameState.board, g_gameData.tetrominoes);

    DrawTetrominoInBoard(graphicsBuffer, &g_gameState.board, &g_gameState.current, &g_gameData.tetrominoes[g_gameState.current.type], OPACITY_NONE);

    DrawTetrominoInBoard(graphicsBuffer, &g_gameState.board, &ghost, &g_gameData.tetrominoes[ghost.type], 192);

    DrawTetrominoInScreen(graphicsBuffer, &g_gameState.next, g_gameState.board.tileSize, &g_gameData.tetrominoes[g_gameState.next.type], OPACITY_NONE);

    DrawTetrominoInScreen(graphicsBuffer, &g_gameState.hold, g_gameState.board.tileSize, &g_gameData.tetrominoes[g_gameState.hold.type], g_gameState.didUseHoldBox ? 192 : OPACITY_NONE);

    DrawRectangle(graphicsBuffer, keyboardState->mouseX, keyboardState->mouseY, 16, 16, 0xFFFFFF);


    ProcessSound(soundBuffer, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
}