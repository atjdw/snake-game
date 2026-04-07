#include <stdio.h>
#include <stdlib.h>
#include <conio.h>  
#include <windows.h>  
#include <time.h>
#include <wchar.h>
#include <stdarg.h>
#include <string.h>

// ==================== 游戏配置 ====================
#define WIDTH  20     // 游戏区域宽度
#define HEIGHT 20    // 游戏区域高度
#define MAX_LENGTH (WIDTH * HEIGHT)  // 蛇的最大长度
#define SAVE_FILE "save.txt"

// 方向枚举
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

// 坐标结构
typedef struct {
    int x;
    int y;
} Position;

// 蛇结构
typedef struct {
    Position body[MAX_LENGTH];  // 蛇身坐标数组
    int length;                 // 当前长度
    Direction dir;              // 当前移动方向
} Snake;

// 游戏状态
typedef struct {
    Snake snake;                // 蛇对象
    Position food;              // 食物位置
    int score;                  // 当前分数
    int gameOver;               // 程序退出标志
    int speed;                  // 游戏速度（毫秒）
    GameState state;            // 当前游戏状态
    int fullWidth;              // 0: 半角模式, 1: 全角模式
    ControllerMode controller;  // 控制模式：玩家/AI
    Direction desiredDir;       // 控制层输出方向
} Game;

// ==================== 函数声明 ====================
void initGame(Game *game);
void setupFood(Game *game);
void processInput(Game *game);
void update(Game *game);
void render(const Game *game);
void runGameLoop(Game *game);
void initConsoleDoubleBuffer(void);
void shutdownConsoleDoubleBuffer(void);
int saveGame(const Game *game);
int loadGame(Game *game);
const char* getStateTextA(GameState state);
const wchar_t* getStateTextW(GameState state);
int getDifficultyLevel(int speed);
GameState nextState(GameState current, GameEvent event);
void dispatchEvent(Game *game, GameEvent event);
int isOppositeDirection(Direction a, Direction b);
int parseDirectionKey(int key, Direction *outDir);
Direction chooseAIDirection(const Game *game);
int isDirectionSafe(const Game *game, Direction dir);
int simulateNextSnake(const Game *game, Direction dir, Snake *outSnake, Position *outHead, int *outAteFood);
int shortestPathDistance(const Snake *snake, Position start, Position target);
int shortestPathDistanceTailAware(const Snake *snake, Position start, Position target, int tailFree);
int reachableArea(const Snake *snake, Position start);
int manhattanDistance(Position a, Position b);
void applyControllerDecision(Game *game);
const char* getControllerTextA(ControllerMode mode);
const wchar_t* getControllerTextW(ControllerMode mode);

// 双缓冲控制台状态
static HANDLE g_originalBuffer = NULL;
static HANDLE g_buffers[2] = {NULL, NULL};
static int g_activeBuffer = 0;
static int g_doubleBufferEnabled = 0;
static COORD g_bufferSize = {0, 0};
static DWORD g_originalConsoleMode = 0;

#define ANSI_RESET   L"\x1b[0m"
#define ANSI_BOLD    L"\x1b[1m"
#define ANSI_DIM     L"\x1b[2m"
#define ANSI_WHITE   L"\x1b[37m"
#define ANSI_GRAY    L"\x1b[90m"
#define ANSI_RED     L"\x1b[91m"
#define ANSI_GREEN   L"\x1b[92m"
#define ANSI_YELLOW  L"\x1b[93m"
#define ANSI_BLUE    L"\x1b[94m"
#define ANSI_CYAN    L"\x1b[96m"

// 追加格式化宽字符串到帧缓冲
static void appendf(wchar_t *buffer, size_t cap, size_t *offset, const wchar_t *fmt, ...) {
    if (*offset >= cap) return;

    va_list args;
    va_start(args, fmt);
    int written = _vsnwprintf(buffer + *offset, cap - *offset, fmt, args);
    va_end(args);

    if (written < 0) {
        buffer[cap - 1] = L'\0';
        *offset = cap - 1;
        return;
    }

    *offset += (size_t)written;
}

static void enableAnsiForHandle(HANDLE handle) {
    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) return;
    if (g_originalConsoleMode == 0) {
        g_originalConsoleMode = mode;
    }
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(handle, mode);
}

const char* getStateTextA(GameState state) {
    switch (state) {
        case STATE_START: return u8"开始";
        case STATE_RUNNING: return u8"进行中";
        case STATE_PAUSED: return u8"暂停";
        case STATE_OVER: return u8"结束";
        default: return u8"未知";
    }
}

const wchar_t* getStateTextW(GameState state) {
    switch (state) {
        case STATE_START: return L"开始";
        case STATE_RUNNING: return L"进行中";
        case STATE_PAUSED: return L"暂停";
        case STATE_OVER: return L"结束";
        default: return L"未知";
    }
}

int getDifficultyLevel(int speed) {
    if (speed >= 90) return 1;
    if (speed >= 75) return 2;
    if (speed >= 60) return 3;
    if (speed >= 45) return 4;
    return 5;
}

GameState nextState(GameState current, GameEvent event) {
    switch (current) {
        case STATE_START:
            if (event == EVENT_START_ACTION || event == EVENT_TOGGLE_PAUSE) return STATE_RUNNING;
            return STATE_START;
        case STATE_RUNNING:
            if (event == EVENT_TOGGLE_PAUSE) return STATE_PAUSED;
            if (event == EVENT_COLLISION) return STATE_OVER;
            return STATE_RUNNING;
        case STATE_PAUSED:
            if (event == EVENT_TOGGLE_PAUSE) return STATE_RUNNING;
            return STATE_PAUSED;
        case STATE_OVER:
            if (event == EVENT_LOAD_GAME) return STATE_PAUSED;
            return STATE_OVER;
        default:
            return STATE_START;
    }
}

void dispatchEvent(Game *game, GameEvent event) {
    if (event == EVENT_QUIT) {
        game->gameOver = 1;
        return;
    }

    game->state = nextState(game->state, event);
}

int isOppositeDirection(Direction a, Direction b) {
    return (a == UP && b == DOWN) ||
           (a == DOWN && b == UP) ||
           (a == LEFT && b == RIGHT) ||
           (a == RIGHT && b == LEFT);
}

int parseDirectionKey(int key, Direction *outDir) {
    switch (key) {
        case 'w':
        case 'W':
            *outDir = UP;
            return 1;
        case 's':
        case 'S':
            *outDir = DOWN;
            return 1;
        case 'a':
        case 'A':
            *outDir = LEFT;
            return 1;
        case 'd':
        case 'D':
            *outDir = RIGHT;
            return 1;
        default:
            return 0;
    }
}

const char* getControllerTextA(ControllerMode mode) {
    return mode == CONTROLLER_AI ? u8"AI" : u8"玩家";
}

const wchar_t* getControllerTextW(ControllerMode mode) {
    return mode == CONTROLLER_AI ? L"AI" : L"玩家";
}

int isDirectionSafe(const Game *game, Direction dir) {
    Position next = game->snake.body[0];

    switch (dir) {
        case UP:    next.y--; break;
        case DOWN:  next.y++; break;
        case LEFT:  next.x--; break;
        case RIGHT: next.x++; break;
        default:    return 0;
    }

    if (next.x < 0 || next.x >= WIDTH || next.y < 0 || next.y >= HEIGHT) {
        return 0;
    }

    for (int i = 0; i < game->snake.length; i++) {
        if (next.x == game->snake.body[i].x && next.y == game->snake.body[i].y) {
            return 0;
        }
    }

    return 1;
}

int simulateNextSnake(const Game *game, Direction dir, Snake *outSnake, Position *outHead, int *outAteFood) {
    if (dir == STOP) return 0;

    Position newHead = game->snake.body[0];
    switch (dir) {
        case UP:    newHead.y--; break;
        case DOWN:  newHead.y++; break;
        case LEFT:  newHead.x--; break;
        case RIGHT: newHead.x++; break;
        default:    return 0;
    }

    if (newHead.x < 0 || newHead.x >= WIDTH || newHead.y < 0 || newHead.y >= HEIGHT) {
        return 0;
    }

    for (int i = 1; i < game->snake.length; i++) {
        if (newHead.x == game->snake.body[i].x && newHead.y == game->snake.body[i].y) {
            return 0;
        }
    }

    Snake next = game->snake;
    int ateFood = (newHead.x == game->food.x && newHead.y == game->food.y);

    if (ateFood && next.length < MAX_LENGTH) {
        next.length++;
    }

    for (int i = next.length - 1; i > 0; i--) {
        next.body[i] = next.body[i - 1];
    }
    next.body[0] = newHead;
    next.dir = dir;

    *outSnake = next;
    *outHead = newHead;
    *outAteFood = ateFood;
    return 1;
}

int shortestPathDistance(const Snake *snake, Position start, Position target) {
    if (start.x == target.x && start.y == target.y) return 0;

    int blocked[HEIGHT][WIDTH] = {{0}};
    int visited[HEIGHT][WIDTH] = {{0}};
    int dist[HEIGHT][WIDTH] = {{0}};
    Position queue[WIDTH * HEIGHT];
    int qh = 0, qt = 0;

    for (int i = 1; i < snake->length; i++) {
        int x = snake->body[i].x;
        int y = snake->body[i].y;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            blocked[y][x] = 1;
        }
    }

    queue[qt++] = start;
    visited[start.y][start.x] = 1;

    const int dx[4] = {0, 0, -1, 1};
    const int dy[4] = {-1, 1, 0, 0};

    while (qh < qt) {
        Position cur = queue[qh++];
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + dx[k];
            int ny = cur.y + dy[k];

            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
            if (visited[ny][nx] || blocked[ny][nx]) continue;

            visited[ny][nx] = 1;
            dist[ny][nx] = dist[cur.y][cur.x] + 1;

            if (nx == target.x && ny == target.y) {
                return dist[ny][nx];
            }

            queue[qt++] = (Position){nx, ny};
        }
    }

    return -1;
}

int shortestPathDistanceTailAware(const Snake *snake, Position start, Position target, int tailFree) {
    if (start.x == target.x && start.y == target.y) return 0;

    int blocked[HEIGHT][WIDTH] = {{0}};
    int visited[HEIGHT][WIDTH] = {{0}};
    int dist[HEIGHT][WIDTH] = {{0}};
    Position queue[WIDTH * HEIGHT];
    int qh = 0, qt = 0;

    for (int i = 1; i < snake->length; i++) {
        if (tailFree && i == snake->length - 1) continue;

        int x = snake->body[i].x;
        int y = snake->body[i].y;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            blocked[y][x] = 1;
        }
    }

    queue[qt++] = start;
    visited[start.y][start.x] = 1;

    const int dx[4] = {0, 0, -1, 1};
    const int dy[4] = {-1, 1, 0, 0};

    while (qh < qt) {
        Position cur = queue[qh++];
        for (int k = 0; k < 4; k++) {
            int nx = cur.x + dx[k];
            int ny = cur.y + dy[k];

            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
            if (visited[ny][nx] || blocked[ny][nx]) continue;

            visited[ny][nx] = 1;
            dist[ny][nx] = dist[cur.y][cur.x] + 1;

            if (nx == target.x && ny == target.y) {
                return dist[ny][nx];
            }

            queue[qt++] = (Position){nx, ny};
        }
    }

    return -1;
}

int reachableArea(const Snake *snake, Position start) {
    int blocked[HEIGHT][WIDTH] = {{0}};
    int visited[HEIGHT][WIDTH] = {{0}};
    Position queue[WIDTH * HEIGHT];
    int qh = 0, qt = 0;
    int area = 0;

    for (int i = 1; i < snake->length; i++) {
        int x = snake->body[i].x;
        int y = snake->body[i].y;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            blocked[y][x] = 1;
        }
    }

    if (blocked[start.y][start.x]) return 0;

    queue[qt++] = start;
    visited[start.y][start.x] = 1;

    const int dx[4] = {0, 0, -1, 1};
    const int dy[4] = {-1, 1, 0, 0};

    while (qh < qt) {
        Position cur = queue[qh++];
        area++;

        for (int k = 0; k < 4; k++) {
            int nx = cur.x + dx[k];
            int ny = cur.y + dy[k];

            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) continue;
            if (visited[ny][nx] || blocked[ny][nx]) continue;

            visited[ny][nx] = 1;
            queue[qt++] = (Position){nx, ny};
        }
    }

    return area;
}

int manhattanDistance(Position a, Position b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

Direction chooseAIDirection(const Game *game) {
    Direction dirs[4] = {UP, DOWN, LEFT, RIGHT};
    Direction bestDir = game->snake.dir;
    int bestScore = -1000000000;
    int bestTier = -1;

    for (int i = 0; i < 4; i++) {
        Direction candidate = dirs[i];
        if (isOppositeDirection(game->snake.dir, candidate)) continue;
        if (!isDirectionSafe(game, candidate)) continue;

        Snake nextSnake;
        Position nextHead;
        int ateFood = 0;
        if (!simulateNextSnake(game, candidate, &nextSnake, &nextHead, &ateFood)) continue;

        int distToFood = shortestPathDistance(&nextSnake, nextHead, game->food);
        Position tail = nextSnake.body[nextSnake.length - 1];
        int distToTail = shortestPathDistanceTailAware(&nextSnake, nextHead, tail, 1);
        int area = reachableArea(&nextSnake, nextHead);
        int safetyNeed = nextSnake.length + 2;

        int score = 0;
        int tier = 0;

        if (distToFood >= 0 && distToTail >= 0 && area >= safetyNeed) {
            tier = 2;
        } else if (distToTail >= 0) {
            tier = 1;
        }

        if (distToFood >= 0) score += 6000 - distToFood * 22;
        else score -= manhattanDistance(nextHead, game->food) * 6;

        if (distToTail >= 0) score += 2200 - distToTail * 10;
        else score -= 4500;

        score += area * 12;

        // 避免钻入面积小于自身长度的陷阱
        if (area < safetyNeed) {
            score -= 5000;
        }

        if (ateFood) {
            score += 120;
        }

        // 轻微偏好保持方向，减少抖动
        if (candidate == game->snake.dir) {
            score += 20;
        }

        if (tier > bestTier || (tier == bestTier && score > bestScore)) {
            bestTier = tier;
            bestScore = score;
            bestDir = candidate;
        }
    }

    return bestDir;
}

void applyControllerDecision(Game *game) {
    if (game->controller == CONTROLLER_AI) {
        game->desiredDir = chooseAIDirection(game);
    }

    if (!isOppositeDirection(game->snake.dir, game->desiredDir)) {
        game->snake.dir = game->desiredDir;
    }
}

int saveGame(const Game *game) {
    FILE *fp = fopen(SAVE_FILE, "w");
    if (!fp) return 0;

    fprintf(fp, "SNAKE_SAVE_V1\n");
    fprintf(fp, "%d %d %d %d %d %d %d %d %d %d\n",
            WIDTH,
            HEIGHT,
            (int)game->state,
            game->fullWidth,
            game->score,
            game->speed,
            game->snake.length,
            (int)game->snake.dir,
            game->food.x,
            game->food.y);

    for (int i = 0; i < game->snake.length; i++) {
        fprintf(fp, "%d %d\n", game->snake.body[i].x, game->snake.body[i].y);
    }

    fclose(fp);
    return 1;
}

int loadGame(Game *game) {
    FILE *fp = fopen(SAVE_FILE, "r");
    if (!fp) return 0;

    char header[64] = {0};
    if (!fgets(header, sizeof(header), fp)) {
        fclose(fp);
        return 0;
    }

    if (strncmp(header, "SNAKE_SAVE_V1", 13) != 0) {
        fclose(fp);
        return 0;
    }

    int saveWidth = 0, saveHeight = 0;
    int state = 0, fullWidth = 0, score = 0, speed = 0, length = 0, dir = 0;
    int foodX = 0, foodY = 0;

    if (fscanf(fp, "%d %d %d %d %d %d %d %d %d %d",
               &saveWidth,
               &saveHeight,
               &state,
               &fullWidth,
               &score,
               &speed,
               &length,
               &dir,
               &foodX,
               &foodY) != 10) {
        fclose(fp);
        return 0;
    }

    if (saveWidth != WIDTH || saveHeight != HEIGHT) {
        fclose(fp);
        return 0;
    }

    if (length < 1 || length > MAX_LENGTH || dir < UP || dir > STOP) {
        fclose(fp);
        return 0;
    }

    if (foodX < 0 || foodX >= WIDTH || foodY < 0 || foodY >= HEIGHT) {
        fclose(fp);
        return 0;
    }

    Snake loadedSnake;
    loadedSnake.length = length;
    loadedSnake.dir = (Direction)dir;

    for (int i = 0; i < length; i++) {
        int x = 0, y = 0;
        if (fscanf(fp, "%d %d", &x, &y) != 2) {
            fclose(fp);
            return 0;
        }

        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
            fclose(fp);
            return 0;
        }

        loadedSnake.body[i].x = x;
        loadedSnake.body[i].y = y;
    }

    fclose(fp);

    game->snake = loadedSnake;
    game->food.x = foodX;
    game->food.y = foodY;
    game->score = score;
    game->speed = speed;
    game->fullWidth = fullWidth ? 1 : 0;
    game->gameOver = 0;
    game->controller = CONTROLLER_HUMAN;
    game->desiredDir = game->snake.dir;

    if (state < STATE_START || state > STATE_OVER) {
        game->state = STATE_PAUSED;
    } else {
        game->state = (GameState)state;
        dispatchEvent(game, EVENT_LOAD_GAME);
    }

    if (game->speed < 20) game->speed = 20;
    if (game->speed > 300) game->speed = 300;

    return 1;
}

// ==================== 主函数 ====================
int main() {
    Game game;

    // 统一控制台为 UTF-8，避免中文与符号乱码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 初始化控制台双缓冲，减少闪屏
    initConsoleDoubleBuffer();
    
    // 初始化随机数种子
    srand((unsigned)time(NULL));
    
    // 初始化游戏
    initGame(&game);
    
    // 标准游戏循环
    runGameLoop(&game);

    // 退出前恢复默认控制台缓冲区
    shutdownConsoleDoubleBuffer();

    return 0;
}

void runGameLoop(Game *game) {
    while (!game->gameOver) {
        processInput(game);  // 输入采集
        if (game->state == STATE_RUNNING) {
            update(game);    // 状态更新
        }
        render(game);        // 渲染输出
        Sleep(game->speed);  // 控制游戏速度
    }
}

// ==================== 功能实现 ====================

void initConsoleDoubleBuffer(void) {
    g_originalBuffer = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_originalBuffer == INVALID_HANDLE_VALUE || g_originalBuffer == NULL) return;

    enableAnsiForHandle(g_originalBuffer);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_originalBuffer, &csbi)) return;

    g_bufferSize = csbi.dwSize;

    for (int i = 0; i < 2; i++) {
        g_buffers[i] = CreateConsoleScreenBuffer(
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CONSOLE_TEXTMODE_BUFFER,
            NULL
        );
        if (g_buffers[i] == INVALID_HANDLE_VALUE || g_buffers[i] == NULL) {
            g_buffers[i] = NULL;
            return;
        }

        SetConsoleScreenBufferSize(g_buffers[i], g_bufferSize);
        SetConsoleWindowInfo(g_buffers[i], TRUE, &csbi.srWindow);
        SetConsoleOutputCP(CP_UTF8);
        enableAnsiForHandle(g_buffers[i]);
    }

    g_activeBuffer = 0;
    SetConsoleActiveScreenBuffer(g_buffers[g_activeBuffer]);
    g_doubleBufferEnabled = 1;
}

void shutdownConsoleDoubleBuffer(void) {
    if (!g_doubleBufferEnabled) return;

    SetConsoleActiveScreenBuffer(g_originalBuffer);
    if (g_originalBuffer && g_originalConsoleMode != 0) {
        SetConsoleMode(g_originalBuffer, g_originalConsoleMode);
    }

    for (int i = 0; i < 2; i++) {
        if (g_buffers[i]) {
            CloseHandle(g_buffers[i]);
            g_buffers[i] = NULL;
        }
    }

    g_doubleBufferEnabled = 0;
}

// 初始化游戏状态
void initGame(Game *game) {
    // 初始化蛇
    game->snake.length = 3;
    game->snake.dir = RIGHT;
    
    // 设置蛇的初始位置（屏幕中央）
    int startX = WIDTH / 2;
    int startY = HEIGHT / 2;
    
    for (int i = 0; i < game->snake.length; i++) {
        game->snake.body[i].x = startX - i;
        game->snake.body[i].y = startY;
    }
    
    // 初始化游戏参数
    game->score = 0;
    game->gameOver = 0;
    game->speed = 100;  // 初始速度（数值越小越快）
    game->state = STATE_START;
    game->fullWidth = 0;  // 默认半角
    game->controller = CONTROLLER_HUMAN;
    game->desiredDir = game->snake.dir;
    
    // 生成第一个食物
    setupFood(game);
}

// 随机生成食物位置
void setupFood(Game *game) {
    int valid;
    do {
        valid = 1;
        game->food.x = rand() % (WIDTH - 2) + 1;
        game->food.y = rand() % (HEIGHT - 2) + 1;
        
        // 确保食物不在蛇身上
        for (int i = 0; i < game->snake.length; i++) {
            if (game->food.x == game->snake.body[i].x && 
                game->food.y == game->snake.body[i].y) {
                valid = 0;
                break;
            }
        }
    } while (!valid);
}

// 绘制游戏画面
void render(const Game *game) {
    if (!g_doubleBufferEnabled) {
        system("cls");
        const char *leftTop = game->fullWidth ? u8"┏" : "+";
        const char *rightTop = game->fullWidth ? u8"┓" : "+";
        const char *leftBottom = game->fullWidth ? u8"┗" : "+";
        const char *rightBottom = game->fullWidth ? u8"┛" : "+";
        const char *hBorder = game->fullWidth ? u8"━" : "-";
        const char *vBorder = game->fullWidth ? u8"┃" : "|";
        const char *headCh = game->fullWidth ? u8"＠" : "@";
        const char *bodyCh = game->fullWidth ? u8"Ｏ" : "o";
        const char *foodCh = game->fullWidth ? u8"＊" : "*";
        const char *emptyCh = game->fullWidth ? u8"　" : " ";
        const char *modeText = game->fullWidth ? u8"全角" : u8"半角";
        const char *stateText = getStateTextA(game->state);
        const char *controllerText = getControllerTextA(game->controller);
        int borderUnits = game->fullWidth ? WIDTH * 2 : WIDTH;

        printf("%ls%ls", ANSI_BOLD, ANSI_CYAN);
        printf("%s", leftTop);
        for (int i = 0; i < borderUnits; i++) printf("%s", hBorder);
        printf("%s%ls  贪吃蛇游戏%ls\n", rightTop, ANSI_RESET, ANSI_RESET);

        for (int y = 0; y < HEIGHT; y++) {
            printf("%ls%s", ANSI_BLUE, vBorder);
            for (int x = 0; x < WIDTH; x++) {
                int isSnake = 0;

                if (x == game->snake.body[0].x && y == game->snake.body[0].y) {
                    printf("%ls%s%ls", ANSI_YELLOW, headCh, ANSI_BLUE);
                    isSnake = 1;
                } else {
                    for (int i = 1; i < game->snake.length; i++) {
                        if (x == game->snake.body[i].x && y == game->snake.body[i].y) {
                            printf("%ls%s%ls", ANSI_GREEN, bodyCh, ANSI_BLUE);
                            isSnake = 1;
                            break;
                        }
                    }
                }

                if (!isSnake && x == game->food.x && y == game->food.y) {
                    printf("%ls%s%ls", ANSI_RED, foodCh, ANSI_BLUE);
                } else if (!isSnake) {
                    printf("%s", emptyCh);
                }
            }
            printf("%ls%s", ANSI_BLUE, vBorder);

            if (y == 2) printf("%ls  分数: %d%ls", ANSI_WHITE, game->score, ANSI_RESET);
            if (y == 4) printf("%ls  长度: %d%ls", ANSI_WHITE, game->snake.length, ANSI_RESET);
            if (y == 6) printf("%ls  速度: %d%ls", ANSI_WHITE, 150 - game->speed, ANSI_RESET);
            if (y == 8) printf("%ls  当前难度: %d级%ls", ANSI_YELLOW, getDifficultyLevel(game->speed), ANSI_RESET);
            if (y == 9) printf("%ls  控制模式: %s%ls", ANSI_CYAN, controllerText, ANSI_RESET);
            if (y == 10) printf("%ls  方向: W/A/S/D%ls", ANSI_GRAY, ANSI_RESET);
            if (y == 11) printf("%ls  切模式: M%ls", ANSI_GRAY, ANSI_RESET);
            if (y == 12) printf("%ls  暂停: P%ls", ANSI_GRAY, ANSI_RESET);
            if (y == 13) printf("%ls  存档: K  读档: L%ls", ANSI_GRAY, ANSI_RESET);
            if (y == 14) printf("%ls  回车: 切换全/半角%ls", ANSI_GRAY, ANSI_RESET);
            if (y == 15) printf("%ls  当前: %s%ls", ANSI_WHITE, modeText, ANSI_RESET);
            if (y == 16) printf("%ls  状态: %s%ls", ANSI_WHITE, stateText, ANSI_RESET);
            if (y == 17) printf("%ls  退出: X%ls", ANSI_RED, ANSI_RESET);
            if (game->state == STATE_START && y == 18) printf("%ls  按任意键开始游戏%ls", ANSI_GREEN, ANSI_RESET);
            if (game->state == STATE_PAUSED && y == 18) printf("%ls  已暂停，按 P 继续%ls", ANSI_YELLOW, ANSI_RESET);
            if (game->state == STATE_OVER && y == 18) printf("%ls  游戏结束，按 L 读档或 X 退出%ls", ANSI_RED, ANSI_RESET);

            printf("\n");
        }

        printf("%ls%ls", ANSI_BOLD, ANSI_CYAN);
        printf("%s", leftBottom);
        for (int i = 0; i < borderUnits; i++) printf("%s", hBorder);
        printf("%s%ls\n", rightBottom, ANSI_RESET);
        return;
    }

    int backBuffer = 1 - g_activeBuffer;
    HANDLE hBack = g_buffers[backBuffer];
    if (!hBack) return;

    wchar_t frame[16384];
    size_t offset = 0;
    frame[0] = L'\0';

    const wchar_t *leftTop = game->fullWidth ? L"┏" : L"+";
    const wchar_t *rightTop = game->fullWidth ? L"┓" : L"+";
    const wchar_t *leftBottom = game->fullWidth ? L"┗" : L"+";
    const wchar_t *rightBottom = game->fullWidth ? L"┛" : L"+";
    const wchar_t *hBorder = game->fullWidth ? L"━" : L"-";
    const wchar_t *vBorder = game->fullWidth ? L"┃" : L"|";
    const wchar_t *modeText = game->fullWidth ? L"全角" : L"半角";
    const wchar_t *stateText = getStateTextW(game->state);
    const wchar_t *controllerText = getControllerTextW(game->controller);
    int borderUnits = game->fullWidth ? WIDTH * 2 : WIDTH;

    wchar_t headCh = game->fullWidth ? L'＠' : L'@';
    wchar_t bodyCh = game->fullWidth ? L'Ｏ' : L'o';
    wchar_t foodCh = game->fullWidth ? L'＊' : L'*';
    wchar_t emptyCh = game->fullWidth ? L'　' : L' ';

    // 顶部边框
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls", ANSI_BOLD, ANSI_CYAN);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", leftTop);
    for (int i = 0; i < borderUnits; i++) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", hBorder);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls  贪吃蛇游戏%ls\n", rightTop, ANSI_RESET, ANSI_RESET);

    // 游戏区域
    for (int y = 0; y < HEIGHT; y++) {
        appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls", ANSI_BLUE, vBorder);
        for (int x = 0; x < WIDTH; x++) {
            wchar_t cell = emptyCh;

            if (x == game->snake.body[0].x && y == game->snake.body[0].y) {
                appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%lc%ls", ANSI_YELLOW, headCh, ANSI_BLUE);
                continue;
            } else {
                for (int i = 1; i < game->snake.length; i++) {
                    if (x == game->snake.body[i].x && y == game->snake.body[i].y) {
                        appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%lc%ls", ANSI_GREEN, bodyCh, ANSI_BLUE);
                        cell = 0;
                        break;
                    }
                }

                if (cell != 0 && x == game->food.x && y == game->food.y) {
                    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%lc%ls", ANSI_RED, foodCh, ANSI_BLUE);
                    cell = 0;
                }
            }

            if (cell != 0) {
                appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%lc", cell);
            }
        }

        appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls", ANSI_BLUE, vBorder);

        if (y == 2) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  分数: %d%ls", ANSI_WHITE, game->score, ANSI_RESET);
        if (y == 4) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  长度: %d%ls", ANSI_WHITE, game->snake.length, ANSI_RESET);
        if (y == 6) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  速度: %d%ls", ANSI_WHITE, 150 - game->speed, ANSI_RESET);
        if (y == 8) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  当前难度: %d级%ls", ANSI_YELLOW, getDifficultyLevel(game->speed), ANSI_RESET);
        if (y == 9) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  控制模式: %ls%ls", ANSI_CYAN, controllerText, ANSI_RESET);
        if (y == 10) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  方向: W/A/S/D%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 11) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  切模式: M%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 12) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  暂停: P%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 13) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  存档: K  读档: L%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 14) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  回车: 切换全/半角%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 15) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  当前: %ls%ls", ANSI_WHITE, modeText, ANSI_RESET);
        if (y == 16) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  状态: %ls%ls", ANSI_WHITE, stateText, ANSI_RESET);
        if (y == 17) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  退出: X%ls", ANSI_RED, ANSI_RESET);
        if (game->state == STATE_START && y == 18) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  按任意键开始游戏%ls", ANSI_GREEN, ANSI_RESET);
        if (game->state == STATE_PAUSED && y == 18) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  已暂停，按 P 继续%ls", ANSI_YELLOW, ANSI_RESET);
        if (game->state == STATE_OVER && y == 18) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  游戏结束，按 L 读档或 X 退出%ls", ANSI_RED, ANSI_RESET);

        appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"\n");
    }

    // 底部边框
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls", ANSI_BOLD, ANSI_CYAN);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", leftBottom);
    for (int i = 0; i < borderUnits; i++) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", hBorder);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls\n", rightBottom, ANSI_RESET);

    // 先清空后台缓冲，再一次性输出整帧
    DWORD written = 0;
    COORD origin = {0, 0};
    FillConsoleOutputCharacterW(hBack, L' ', (DWORD)(g_bufferSize.X * g_bufferSize.Y), origin, &written);
    SetConsoleCursorPosition(hBack, origin);
    WriteConsoleW(hBack, frame, (DWORD)wcslen(frame), &written, NULL);

    // 交换前后台缓冲
    SetConsoleActiveScreenBuffer(hBack);
    g_activeBuffer = backBuffer;
}

// 键盘输入处理（同帧多输入，保留最后一个合法方向）
void processInput(Game *game) {
    Direction pendingDir = game->snake.dir;
    int hasPendingDir = 0;

    while (_kbhit()) {
        int key = _getch();

        // 扩展按键（方向键等）读取第二字节，仍可触发开局
        if (key == 0 || key == 224) {
            if (_kbhit()) _getch();
            if (game->state == STATE_START) dispatchEvent(game, EVENT_START_ACTION);
            continue;
        }

        // 回车切换全角/半角模式
        if (key == '\r') {
            game->fullWidth = !game->fullWidth;
            if (game->state == STATE_START) dispatchEvent(game, EVENT_START_ACTION);
            continue;
        }

        if (key == 'k' || key == 'K') {
            saveGame(game);
            continue;
        }

        if (key == 'l' || key == 'L') {
            loadGame(game);
            continue;
        }

        if (key == 'p' || key == 'P') {
            dispatchEvent(game, EVENT_TOGGLE_PAUSE);
            continue;
        }

        if (key == 'm' || key == 'M') {
            game->controller = (game->controller == CONTROLLER_HUMAN) ? CONTROLLER_AI : CONTROLLER_HUMAN;
            game->desiredDir = game->snake.dir;
            continue;
        }

        if (key == 'x' || key == 'X') {
            dispatchEvent(game, EVENT_QUIT);
            return;
        }

        // 首次任意键开局
        if (game->state == STATE_START) {
            dispatchEvent(game, EVENT_START_ACTION);
            continue;
        }

        if (game->state != STATE_RUNNING || game->controller != CONTROLLER_HUMAN) continue;

        Direction candidate;
        if (parseDirectionKey(key, &candidate)) {
            Direction baseDir = hasPendingDir ? pendingDir : game->snake.dir;
            if (!isOppositeDirection(baseDir, candidate)) {
                pendingDir = candidate;
                hasPendingDir = 1;
            }
        }
    }

    if (hasPendingDir) {
        game->desiredDir = pendingDir;
    }
}

// 游戏逻辑更新
void update(Game *game) {
    applyControllerDecision(game);

    // 1. 计算新蛇头位置
    Position newHead = game->snake.body[0];
    
    switch (game->snake.dir) {
        case UP:    newHead.y--; break;
        case DOWN:  newHead.y++; break;
        case LEFT:  newHead.x--; break;
        case RIGHT: newHead.x++; break;
        case STOP:  return;
    }
    
    // 2. 检查碰撞
    // 撞墙检测
    if (newHead.x < 0 || newHead.x >= WIDTH || 
        newHead.y < 0 || newHead.y >= HEIGHT) {
        dispatchEvent(game, EVENT_COLLISION);
        return;
    }
    
    // 撞自己检测（从1开始，不检查蛇头自身）
    for (int i = 1; i < game->snake.length; i++) {
        if (newHead.x == game->snake.body[i].x && 
            newHead.y == game->snake.body[i].y) {
            dispatchEvent(game, EVENT_COLLISION);
            return;
        }
    }
    
    // 3. 检查是否吃到食物
    int ateFood = (newHead.x == game->food.x && newHead.y == game->food.y);
    
    // 4. 移动蛇身

    
    if (!ateFood) {
        // 身体后移（从尾部开始）
        for (int i = game->snake.length - 1; i > 0; i--) {
            game->snake.body[i] = game->snake.body[i - 1];
        }
    } else {
        // 增长蛇身
        if (game->snake.length < MAX_LENGTH) {
            game->snake.length++;
            // 后移为新节点腾出位置
            for (int i = game->snake.length - 1; i > 0; i--) {
                game->snake.body[i] = game->snake.body[i - 1];
            }
            // 增加分数和速度
            game->score += 10;
            if (game->speed > 30) game->speed -= 2;  // 逐渐加速
            // 生成新食物
            setupFood(game);
        }
    }
    
    // 更新蛇头位置
    game->snake.body[0] = newHead;
}
