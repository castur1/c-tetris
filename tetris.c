#include "tetris.h"
#include "tetris_graphics.h"
#include "tetris_sound.h"
#include "tetris_random.h"


#define AUDIO_CHANNEL_COUNT 32

#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20

#define AUTO_MOVE_DELAY 0.2f
#define AUTO_MOVE       0.05f
#define SOFT_DROP       0.033f
#define LOCK_DELAY      0.5f

#define BACKGROUND_MUSIC 0.6f
#define SFX_MOVE         1.0f
#define SFX_ROTATE       1.5f
#define SFX_LOCK         2.0f
#define SFX_LINE_CLEAR   1.5f
#define SFX_HOLD         2.0f
#define SFX_LEVEL_UP     1.5f
#define SFX_SOFT_DROP    0.8f

#define SCORE_SINGLE    40
#define SCORE_DOUBLE    100
#define SCORE_TRIPLE    300
#define SCORE_QUADRUPLE 1200
#define SCORE_SOFT_DROP 1
#define SCORE_HARD_DROP 2


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

typedef struct tetromino_t {
    tetromino_type type;
    i32 rotation;
    i32 x;
    i32 y;
} tetromino_t;

typedef enum button_state {
    button_state_idle,
    button_state_hover,
    button_state_pressed,
    button_state_held,
    button_state_released
} button_state;

typedef struct button_t {
    i32 x;
    i32 y;
    i32 width;
    i32 height;
    button_state state;
} button_t;

typedef struct font_t {
    bitmap_buffer spriteSheet;
    i32 sheetWidth;
    i32 sheetHeight;
    char* characters;
    i32 charactersCount;
    // Individual offsets etc.
} font_t;

typedef void (*scene_pointer)(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime);

typedef struct game_state {
    scene_pointer currentScene;

    audio_channel audioChannels[AUDIO_CHANNEL_COUNT];
    f32 audioVolume;

    i32 highScore;

    // Scene 1 //

    f32 timerFall;
    f32 timerAutoMoveDelay;
    f32 timerAutoMove;
    f32 timerLockDelay;

    board_t board;
    tetromino_t current;
    tetromino_t next[3];
    tetromino_t hold;
    b32 didUseHoldBox;
    tetromino_type bag[7];
    i32 bagIndex;
    i32 score;
    i32 level;
    i32 lines;

    button_t buttonPause; // Do we really need to remember the button?

    // Scene 2 //

    // Scene 3 //

    audio_channel tempAudioChannels[AUDIO_CHANNEL_COUNT];

} game_state;

typedef struct game_data {
    // Scene 1 //

    bitmap_buffer tetrominoes[8];
    bitmap_buffer tetrominoesUI[8];

    bitmap_buffer background;

    bitmap_buffer digits[10];
    font_t font;

    sound_buffer backgroundMusic;
    sound_buffer sfxMove;
    sound_buffer sfxRotate;
    sound_buffer sfxLock;
    sound_buffer sfxLineClear;
    sound_buffer sfxHold;
    sound_buffer sfxLevelUp;
    sound_buffer sfxSoftDrop;
    // Etc.?

    // Scene 2 //

} game_data;

typedef struct save_data {
    i32 highScore;
} save_data;


static game_state g_gameState;
static game_data g_gameData;


static void InitScene1(void);
static void InitScene2(void);
static void InitScene3(void);
static void Scene1(bitmap_buffer*, sound_buffer*, keyboard_state*, f32);
static void Scene2(bitmap_buffer*, sound_buffer*, keyboard_state*, f32);
static void Scene3(bitmap_buffer*, sound_buffer*, keyboard_state*, f32);
static void CloseScene1(void);
static void CloseScene2(void);
static void CloseScene3(void);


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
                DrawBitmap(graphicsBuffer, &sprites[tile], board->x + x * board->tileSize, board->y + y * board->tileSize, board->tileSize, 255);
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

static i32 ProcessLineClears(board_t* board, tetromino_t* tetromino) {
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
    for (i32 i = 0; i < 7;) {
        i32 attempt = RandomI32InRange(1, 7);
        for (i32 j = 0; j < i; ++j) {
            if (bag[j] == attempt) {
                attempt = -1;
                break;
            }
        }
        if (attempt == -1) {
            continue;
        }
        bag[i] = attempt;
        ++i;
    }
}

static tetromino_type GetNextTetrominoFromBag(tetromino_type* bag, i32* bagIndex) {
    tetromino_type result = bag[(*bagIndex)++];

    if (*bagIndex >= 7) {
        *bagIndex = 0;
        RandomizeBag(bag);
    }

    return result;
}

static f32 GetCurrentGravityInSeconds(i32 level) {
    f32 gravityInSeconds = 1.0f;
    f32 base = 0.8f - (level - 1) * 0.007f;
    for (i32 i = 0; i < level - 1; ++i) {
        gravityInSeconds *= base;
    }

    return gravityInSeconds;
}

static save_data ReadSaveData(const char* filePath) {
    i32 bytesRead = 0;
    void* contents = EngineReadEntireFile(filePath, &bytesRead);

    save_data data;
    if (bytesRead != sizeof(save_data)) {
        data = (save_data){ .highScore = 0 };
        EngineWriteEntireFile(filePath, &data, sizeof(save_data));
    }
    else {
        data = *(save_data*)contents;
    }
    
    EngineFree(contents);

    return data;
    
}

static inline b32 IsPointInRect(i32 px, i32 py, i32 rxl, i32 ryl, i32 rxr, i32 ryr) {
    return px >= rxl && px < rxr && py >= ryl && py < ryr;
}

// Wtf even is this lol
static void UpdateButtonState(button_t* button, i32 mouseX, i32 mouseY, keyboard_key_state* mouseButton) {
    if (IsPointInRect(mouseX, mouseY, button->x, button->y, button->x + button->width, button->y + button->height)) {
        if (mouseButton->isDown) {
            if (mouseButton->didChangeState) {
                button->state = button_state_pressed;
            }
            else if (button->state == button_state_held || button->state == button_state_pressed) {
                button->state = button_state_held;
            }
            else {
                button->state = button_state_hover;
            }
        }
        else {
            if (button->state == button_state_held || button->state == button_state_pressed) {
                button->state = button_state_released;
            }
            else {
                button->state = button_state_hover;
            }
        }
    }
    else {
        if (button->state == button_state_held || button->state == button_state_pressed) {
            button->state = button_state_released;
        }
        else {
            button->state = button_state_idle;
        }
    }
}

// SCENE 1 //

static void InitScene1(void) {
    g_gameData.tetrominoes[1] = LoadBMP("assets/graphics/tetrominoes/tetromino_i.bmp");
    g_gameData.tetrominoes[2] = LoadBMP("assets/graphics/tetrominoes/tetromino_o.bmp");
    g_gameData.tetrominoes[3] = LoadBMP("assets/graphics/tetrominoes/tetromino_t.bmp");
    g_gameData.tetrominoes[4] = LoadBMP("assets/graphics/tetrominoes/tetromino_s.bmp");
    g_gameData.tetrominoes[5] = LoadBMP("assets/graphics/tetrominoes/tetromino_z.bmp");
    g_gameData.tetrominoes[6] = LoadBMP("assets/graphics/tetrominoes/tetromino_j.bmp");
    g_gameData.tetrominoes[7] = LoadBMP("assets/graphics/tetrominoes/tetromino_l.bmp");

    g_gameData.tetrominoesUI[1] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_I_UI.bmp");
    g_gameData.tetrominoesUI[2] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_O_UI.bmp");
    g_gameData.tetrominoesUI[3] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_T_UI.bmp");
    g_gameData.tetrominoesUI[4] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_S_UI.bmp");
    g_gameData.tetrominoesUI[5] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_Z_UI.bmp");
    g_gameData.tetrominoesUI[6] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_J_UI.bmp");
    g_gameData.tetrominoesUI[7] = LoadBMP("assets/graphics/tetrominoes_ui/tetromino_L_UI.bmp");

    g_gameData.background = LoadBMP("assets/graphics/background3.bmp");

    // Change font?
    g_gameData.digits[0] = LoadBMP("assets/graphics/digits/digit_0.bmp");
    g_gameData.digits[1] = LoadBMP("assets/graphics/digits/digit_1.bmp");
    g_gameData.digits[2] = LoadBMP("assets/graphics/digits/digit_2.bmp");
    g_gameData.digits[3] = LoadBMP("assets/graphics/digits/digit_3.bmp");
    g_gameData.digits[4] = LoadBMP("assets/graphics/digits/digit_4.bmp");
    g_gameData.digits[5] = LoadBMP("assets/graphics/digits/digit_5.bmp");
    g_gameData.digits[6] = LoadBMP("assets/graphics/digits/digit_6.bmp");
    g_gameData.digits[7] = LoadBMP("assets/graphics/digits/digit_7.bmp");
    g_gameData.digits[8] = LoadBMP("assets/graphics/digits/digit_8.bmp");
    g_gameData.digits[9] = LoadBMP("assets/graphics/digits/digit_9.bmp");

    g_gameData.font = (font_t){
        .spriteSheet = LoadBMP("assets/letters_sprite_sheet.bmp"),
        .sheetWidth = 13,
        .sheetHeight = 5,
        .characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz,.-",
        .charactersCount = 65
    };

    g_gameData.backgroundMusic = LoadWAV("assets/audio/Tetris3.wav");

    // Look these over
    g_gameData.sfxMove      = LoadWAV("assets/audio/sfx1.wav");
    g_gameData.sfxRotate    = LoadWAV("assets/audio/sfx4.wav");
    g_gameData.sfxLock      = LoadWAV("assets/audio/sfx3.wav");
    g_gameData.sfxLineClear = LoadWAV("assets/audio/sfx5.wav"); 
    g_gameData.sfxHold      = LoadWAV("assets/audio/sfx2.wav");
    g_gameData.sfxLevelUp   = LoadWAV("assets/audio/sfx6.wav");
    g_gameData.sfxSoftDrop  = LoadWAV("assets/audio/sfx1.wav");

    g_gameState.audioVolume = 1.0f;

    PlaySound(&g_gameData.backgroundMusic, true, BACKGROUND_MUSIC, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);

    RandomInit();

    g_gameState.board = InitBoard(BOARD_WIDTH, BOARD_HEIGHT, 735, 90, 45);

    RandomizeBag(g_gameState.bag);

    g_gameState.current = InitTetromino(GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex), 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4);
    g_gameState.next[0] = InitTetromino(GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex), 0, 1298, 788);
    g_gameState.next[1] = InitTetromino(GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex), 0, 1298, 653);
    g_gameState.next[2] = InitTetromino(GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex), 0, 1298, 518);
    g_gameState.hold = InitTetromino(tetromino_type_empty, 0, 533, 788);

    g_gameState.score = 0;
    g_gameState.level = 1;
    g_gameState.lines = 0;

    save_data data = ReadSaveData("data/data.txt");
    g_gameState.highScore = data.highScore;

    g_gameState.buttonPause = (button_t){
        .x      = 1770,
        .y      = 50,
        .width  = 100,
        .height = 100,
        .state  = button_state_idle
    };
}

static void CloseScene1(void) {
    EngineFree(g_gameData.tetrominoes[1].memory);
    EngineFree(g_gameData.tetrominoes[2].memory);
    EngineFree(g_gameData.tetrominoes[3].memory);
    EngineFree(g_gameData.tetrominoes[4].memory);
    EngineFree(g_gameData.tetrominoes[5].memory);
    EngineFree(g_gameData.tetrominoes[6].memory);
    EngineFree(g_gameData.tetrominoes[7].memory);

    EngineFree(g_gameData.tetrominoesUI[1].memory);
    EngineFree(g_gameData.tetrominoesUI[2].memory);
    EngineFree(g_gameData.tetrominoesUI[3].memory);
    EngineFree(g_gameData.tetrominoesUI[4].memory);
    EngineFree(g_gameData.tetrominoesUI[5].memory);
    EngineFree(g_gameData.tetrominoesUI[6].memory);
    EngineFree(g_gameData.tetrominoesUI[7].memory);

    EngineFree(g_gameData.background.memory);

    EngineFree(g_gameData.digits[0].memory);
    EngineFree(g_gameData.digits[1].memory);
    EngineFree(g_gameData.digits[2].memory);
    EngineFree(g_gameData.digits[3].memory);
    EngineFree(g_gameData.digits[4].memory);
    EngineFree(g_gameData.digits[5].memory);
    EngineFree(g_gameData.digits[6].memory);
    EngineFree(g_gameData.digits[7].memory);
    EngineFree(g_gameData.digits[8].memory);
    EngineFree(g_gameData.digits[9].memory);

    EngineFree(g_gameData.font.spriteSheet.memory);

    EngineFree(g_gameData.backgroundMusic.samples);
    EngineFree(g_gameData.sfxMove.samples);
    EngineFree(g_gameData.sfxRotate.samples);
    EngineFree(g_gameData.sfxLock.samples);
    EngineFree(g_gameData.sfxLineClear.samples);
    EngineFree(g_gameData.sfxHold.samples);
    EngineFree(g_gameData.sfxLevelUp.samples);
    EngineFree(g_gameData.sfxSoftDrop.samples);

    EngineFree(g_gameState.board.tiles);

    StopAllSounds(g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
}

static void Scene1(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    if (PRESSED(keyboardState->esc)) {
        g_gameState.currentScene = &Scene3;
        InitScene3();
        return;
    }

    if (keyboardState->right.isDown) {
        g_gameState.timerAutoMoveDelay += deltaTime;
        if (g_gameState.timerAutoMoveDelay >= AUTO_MOVE_DELAY) {
            g_gameState.timerAutoMove += deltaTime;
        }
        if (g_gameState.timerAutoMove >= AUTO_MOVE || keyboardState->right.didChangeState) {
            g_gameState.timerAutoMove = 0.0f;
            ++g_gameState.current.x;
            if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                --g_gameState.current.x;
            }
            else {
                PlaySound(&g_gameData.sfxMove, false, SFX_MOVE, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }
    else if (keyboardState->left.isDown) {
        g_gameState.timerAutoMoveDelay += deltaTime;
        if (g_gameState.timerAutoMoveDelay >= AUTO_MOVE_DELAY) {
            g_gameState.timerAutoMove += deltaTime;
        }
        if (g_gameState.timerAutoMove >= AUTO_MOVE || keyboardState->left.didChangeState) {
            g_gameState.timerAutoMove = 0.0f;
            --g_gameState.current.x;
            if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                ++g_gameState.current.x;
            }
            else {
                PlaySound(&g_gameData.sfxMove, false, SFX_MOVE, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }
    else {
        g_gameState.timerAutoMoveDelay = 0.0f;
    }

    i32 rotationDirection = PRESSED(keyboardState->x) - PRESSED(keyboardState->z);
    if (rotationDirection) {
        b32 didRotate = true;

        g_gameState.current.rotation = (g_gameState.current.rotation + rotationDirection + 4) % 4;
        if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
            g_gameState.current.x += 1;
            if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                g_gameState.current.x -= 2;
                if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                    g_gameState.current.x += 1;
                    g_gameState.current.y += 1;
                    if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                        g_gameState.current.y -= 1;
                        g_gameState.current.rotation = (g_gameState.current.rotation - rotationDirection + 4) % 4;
                        didRotate = false;
                    }
                }
            }
        }

        if (didRotate) {
            PlaySound(&g_gameData.sfxRotate, false, SFX_ROTATE, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
        }
    }

    if (PRESSED(keyboardState->c) && !g_gameState.didUseHoldBox) {
        g_gameState.didUseHoldBox = true;

        tetromino_type currentType = g_gameState.current.type;
        if (g_gameState.hold.type == tetromino_type_empty) {
            g_gameState.current = InitTetromino(g_gameState.next[0].type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_gameData.tetrominoes);
            g_gameState.next[0].type = g_gameState.next[1].type;
            g_gameState.next[1].type = g_gameState.next[2].type;
            g_gameState.next[2].type = GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex);
        }
        else {
            g_gameState.current = InitTetromino(g_gameState.hold.type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_gameData.tetrominoes);
        }
        g_gameState.hold.type = currentType;

        PlaySound(&g_gameData.sfxHold, false, SFX_HOLD, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
    }

    b32 didSoftDrop = false;
    f32 gravityInSeconds = GetCurrentGravityInSeconds(g_gameState.level);
    if (keyboardState->down.isDown && gravityInSeconds > SOFT_DROP) {
        didSoftDrop = true;
        gravityInSeconds = SOFT_DROP;
    }

    b32 didHardDrop = false;
    if (PRESSED(keyboardState->up)) {
        didHardDrop = true;
        while (IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
            --g_gameState.current.y;
            g_gameState.score += SCORE_HARD_DROP * g_gameState.level;
        }
        ++g_gameState.current.y;
        g_gameState.score -= SCORE_HARD_DROP * g_gameState.level;
    }

    g_gameState.timerFall += deltaTime;
    if (g_gameState.timerFall >= gravityInSeconds || didHardDrop || g_gameState.timerLockDelay >= 0.001f) {
        g_gameState.timerFall = 0.0f;

        --g_gameState.current.y;

        if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
            ++g_gameState.current.y;

            g_gameState.timerLockDelay += deltaTime;
            if (g_gameState.timerLockDelay >= LOCK_DELAY || didHardDrop) {
                PlaceTetromino(&g_gameState.board, &g_gameState.current);

                i32 lineClearCount = ProcessLineClears(&g_gameState.board, &g_gameState.current);
                g_gameState.lines += lineClearCount;
                switch (lineClearCount) {
                    case 1: {
                        g_gameState.score += SCORE_SINGLE * g_gameState.level;
                    } break;
                    case 2: {
                        g_gameState.score += SCORE_DOUBLE * g_gameState.level;
                    } break;
                    case 3: {
                        g_gameState.score += SCORE_TRIPLE * g_gameState.level;
                    } break;
                    case 4: {
                        g_gameState.score += SCORE_QUADRUPLE * g_gameState.level;
                    } break;
                }


                if (g_gameState.lines >= g_gameState.level * 10) {
                    ++g_gameState.level;
                    PlaySound(&g_gameData.sfxLevelUp, false, SFX_LEVEL_UP, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
                }
                else {
                    switch (lineClearCount) {
                        case 1: {
                            PlaySound(&g_gameData.sfxLineClear, false, SFX_LINE_CLEAR, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 2: {
                            PlaySound(&g_gameData.sfxLineClear, false, SFX_LINE_CLEAR, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 3: {
                            PlaySound(&g_gameData.sfxLineClear, false, SFX_LINE_CLEAR, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                        case 4: {
                            PlaySound(&g_gameData.sfxLineClear, false, SFX_LINE_CLEAR, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;

                        default: {
                            PlaySound(&g_gameData.sfxLock, false, SFX_LOCK, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
                        } break;
                    }
                }

                g_gameState.current = InitTetromino(g_gameState.next[0].type, 0, BOARD_WIDTH / 2 - 2, BOARD_HEIGHT - 4, &g_gameData.tetrominoes);
                g_gameState.next[0].type = g_gameState.next[1].type;
                g_gameState.next[1].type = g_gameState.next[2].type;
                g_gameState.next[2].type = GetNextTetrominoFromBag(g_gameState.bag, &g_gameState.bagIndex);

                if (!IsTetrominoPosValid(&g_gameState.board, &g_gameState.current)) {
                    if (g_gameState.score > g_gameState.highScore) {
                        g_gameState.highScore = g_gameState.score;

                        save_data data = ReadSaveData("data/data.txt");
                        data.highScore = g_gameState.highScore;
                        EngineWriteEntireFile("data/data.txt", &data, sizeof(save_data));
                    }

                    g_gameState.currentScene = &Scene2;
                    CloseScene1();
                    InitScene2();
                    return;
                }

                g_gameState.timerAutoMoveDelay = 0.0f;
                g_gameState.timerLockDelay = 0.0f;
                g_gameState.didUseHoldBox = false;
            }
        }
        else {
            g_gameState.timerLockDelay = 0.0f;

            if (didSoftDrop) {
                g_gameState.score += SCORE_SOFT_DROP * g_gameState.level;
                PlaySound(&g_gameData.sfxSoftDrop, false, SFX_SOFT_DROP, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
            }
        }
    }

    tetromino_t ghost = g_gameState.current;
    while (IsTetrominoPosValid(&g_gameState.board, &ghost)) {
        --ghost.y;
    }
    ++ghost.y;

    DrawBitmapStupid(graphicsBuffer, &g_gameData.background, 0, 0);

    DrawBoard(graphicsBuffer, &g_gameState.board, g_gameData.tetrominoes);

    DrawTetrominoInBoard(graphicsBuffer, &g_gameState.board, &g_gameState.current, &g_gameData.tetrominoes[g_gameState.current.type], 255);

    DrawTetrominoInBoard(graphicsBuffer, &g_gameState.board, &ghost, &g_gameData.tetrominoes[ghost.type], 64); // <-- Feedback :)

    // All these could really be replaced by DrawBitmapStupidButWithOpacity if you think about it.
    // Should save some processing time ig
    DrawBitmap(graphicsBuffer, &g_gameData.tetrominoesUI[g_gameState.next[0].type], g_gameState.next[0].x, g_gameState.next[0].y, 90, 255);
    DrawBitmap(graphicsBuffer, &g_gameData.tetrominoesUI[g_gameState.next[1].type], g_gameState.next[1].x, g_gameState.next[1].y, 90, 255);
    DrawBitmap(graphicsBuffer, &g_gameData.tetrominoesUI[g_gameState.next[2].type], g_gameState.next[2].x, g_gameState.next[2].y, 90, 255);

    DrawBitmap(graphicsBuffer, &g_gameData.tetrominoesUI[g_gameState.hold.type], g_gameState.hold.x, g_gameState.hold.y, 90, g_gameState.didUseHoldBox ? 128 : 255);

    DrawNumber(graphicsBuffer, g_gameState.level, 578, 328, 15, 2, true, g_gameData.digits);
    DrawNumber(graphicsBuffer, g_gameState.score, 578, 238, 15, 2, true, g_gameData.digits);
    DrawNumber(graphicsBuffer, g_gameState.lines, 578, 148, 15, 2, true, g_gameData.digits);

    DrawNumber(graphicsBuffer, g_gameState.highScore, 578, 463, 15, 2, true, g_gameData.digits);

    UpdateButtonState(&g_gameState.buttonPause, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    if (g_gameState.buttonPause.state == button_state_pressed) {
        g_gameState.currentScene = &Scene3;
        InitScene3();
    }

    u32 buttonColour = 0;
    switch (g_gameState.buttonPause.state) {
        case button_state_idle: {
            buttonColour = 0xFF0000;
        } break;
        case button_state_hover: {
            buttonColour = 0x00FF00;
        } break;
        case button_state_pressed: {
            buttonColour = 0x000000;
        } break;
        case button_state_held: {
            buttonColour = 0x0000FF;
        } break;
        case button_state_released: {
            buttonColour = 0xFFFFFF;
        } break;
    }
    DrawRectangle(graphicsBuffer, g_gameState.buttonPause.x, g_gameState.buttonPause.y, g_gameState.buttonPause.width, g_gameState.buttonPause.height, buttonColour);

    const char* testString = "Hello, world";
    i32 testStringLength = 22;
    i32 characterWidth = g_gameData.font.spriteSheet.width / g_gameData.font.sheetWidth;
    i32 characterHeight = g_gameData.font.spriteSheet.height / g_gameData.font.sheetHeight;
    i32 strX = 10;
    i32 strY = 50;
    for (i32 i = 0; i < testStringLength; ++i) {
        i32 index = 0;
        while (testString[i] != g_gameData.font.characters[index] && index < g_gameData.font.charactersCount) {
            ++index;
        } 
        if (index >= g_gameData.font.charactersCount) {
            strX += characterWidth;
            continue;
        }

        i32 sourceX = (index % g_gameData.font.sheetWidth) * characterWidth;
        i32 sourceY = (g_gameData.font.sheetHeight - index / g_gameData.font.sheetWidth - 1) * characterHeight;

        DrawPartialBitmap(graphicsBuffer, &g_gameData.font.spriteSheet, strX, strY, sourceX, sourceY, characterWidth, characterHeight, 255);
        strX += characterWidth;
    }

    extern u32 DEBUG_microsecondsElapsed;
    DrawNumber(graphicsBuffer, DEBUG_microsecondsElapsed, 10, 10, 15, 2, false, g_gameData.digits);
}

// SCENE 2 //

static void InitScene2(void) {}

static void CloseScene2(void) {
    StopAllSounds(g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
}

static void Scene2(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    DrawRectangle(graphicsBuffer, 0, 0, graphicsBuffer->width, graphicsBuffer->height, 0xFFFFFF);
    DrawRectangle(graphicsBuffer, keyboardState->mouseX, keyboardState->mouseY, 16, 16, 0x000000);

    if (PRESSED(keyboardState->mouseLeft)) {
        g_gameState.currentScene = &Scene1;
        CloseScene2();
        InitScene1();
        return;
    }
}

// SCENE 3 //

static void InitScene3(void) {
    CopyAudioChannels(g_gameState.tempAudioChannels, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
    StopAllSounds(g_gameState.audioChannels, AUDIO_CHANNEL_COUNT);
}

static void CloseScene3(void) {
    CopyAudioChannels(g_gameState.audioChannels, g_gameState.tempAudioChannels, AUDIO_CHANNEL_COUNT);
}

static void Scene3(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    UpdateButtonState(&g_gameState.buttonPause, keyboardState->mouseX, keyboardState->mouseY, &keyboardState->mouseLeft);

    u32 buttonColour = 0;
    switch (g_gameState.buttonPause.state) {
        case button_state_idle: {
            buttonColour = 0xFF0000;
        } break;
        case button_state_hover: {
            buttonColour = 0x00FF00;
        } break;
        case button_state_pressed: {
            buttonColour = 0x000000;
        } break;
        case button_state_held: {
            buttonColour = 0x0000FF;
        } break;
        case button_state_released: {
            buttonColour = 0xFFFFFF;
        } break;
    }
    DrawRectangle(graphicsBuffer, g_gameState.buttonPause.x, g_gameState.buttonPause.y, g_gameState.buttonPause.width, g_gameState.buttonPause.height, buttonColour);

    DrawRectangle(graphicsBuffer, keyboardState->mouseX, keyboardState->mouseY, 16, 16, 0xFFFFFF);

    if (PRESSED(keyboardState->esc) || g_gameState.buttonPause.state == button_state_pressed) {
        CloseScene3();
        g_gameState.currentScene = &Scene1;
        return;
    }
}


void OnStartup(void) {
    InitScene1();
    g_gameState.currentScene = &Scene1;
}

void Update(bitmap_buffer* graphicsBuffer, sound_buffer* soundBuffer, keyboard_state* keyboardState, f32 deltaTime) {
    if (PRESSED(keyboardState->f)) {
        EngineToggleFullscreen();
    }

    (*g_gameState.currentScene)(graphicsBuffer, soundBuffer, keyboardState, deltaTime);
    ProcessSound(soundBuffer, g_gameState.audioChannels, AUDIO_CHANNEL_COUNT, g_gameState.audioVolume);
}