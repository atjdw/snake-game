#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <windows.h>

#define WIDTH 20
#define HEIGHT 20
#define MAX_LENGTH (WIDTH * HEIGHT)
#define SAVE_FILE "save.txt"

typedef enum {
    UP = 0,
    DOWN,
    LEFT,
    RIGHT,
    STOP
} Direction;

typedef enum {
    STATE_START = 0,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_OVER
} GameState;

typedef enum {
    EVENT_START_ACTION = 0,
    EVENT_TOGGLE_PAUSE,
    EVENT_COLLISION,
    EVENT_LOAD_GAME,
    EVENT_QUIT
} GameEvent;

typedef enum {
    CONTROLLER_HUMAN = 0,
    CONTROLLER_AI
} ControllerMode;

typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    Position body[MAX_LENGTH];
    int length;
    Direction dir;
} Snake;

typedef struct {
    Snake snake;
    Position food;
    int score;
    int gameOver;
    int speed;
    GameState state;
    int fullWidth;
    ControllerMode controller;
    Direction desiredDir;
} Game;

void initGame(Game *game);
void runGameLoop(Game *game);

void processInput(Game *game);
void render(const Game *game);
void update(Game *game);
void initConsoleDoubleBuffer(void);
void shutdownConsoleDoubleBuffer(void);
void initRandomSeed(void);
void sleepFrame(int milliseconds);

void setupFood(Game *game);
Direction chooseAIDirection(const Game *game);
void applyControllerDecision(Game *game);

int saveGame(const Game *game);
int loadGame(Game *game);

GameState nextState(GameState current, GameEvent event);
void dispatchEvent(Game *game, GameEvent event);
int getDifficultyLevel(int speed);
const char *getStateTextA(GameState state);
const wchar_t *getStateTextW(GameState state);
const char *getControllerTextA(ControllerMode mode);
const wchar_t *getControllerTextW(ControllerMode mode);

int isOppositeDirection(Direction a, Direction b);
int parseDirectionKey(int key, Direction *outDir);

#endif
