#include <stdio.h>
#include <stdlib.h>
#include <conio.h>    // Windows: _kbhit(), _getch()
#include <windows.h>  // Windows: Sleep()
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
} Game;

// ==================== 函数声明 ====================
void initGame(Game *game);
void setupFood(Game *game);
void draw(const Game *game);
void input(Game *game);
void logic(Game *game);
void initConsoleDoubleBuffer(void);
void shutdownConsoleDoubleBuffer(void);
int saveGame(const Game *game);
int loadGame(Game *game);
const char* getStateTextA(GameState state);
const wchar_t* getStateTextW(GameState state);

// 双缓冲控制台状态
static HANDLE g_originalBuffer = NULL;
static HANDLE g_buffers[2] = {NULL, NULL};
static int g_activeBuffer = 0;
static int g_doubleBufferEnabled = 0;
static COORD g_bufferSize = {0, 0};

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

    if (state < STATE_START || state > STATE_OVER) {
        game->state = STATE_PAUSED;
    } else {
        game->state = (GameState)state;
        if (game->state == STATE_OVER) game->state = STATE_PAUSED;
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
    
    // 主游戏循环
    while (!game.gameOver) {
        draw(&game);
        input(&game);
        if (game.state == STATE_RUNNING) {
            logic(&game);
        }
        Sleep(game.speed);  // 控制游戏速度
    }

    // 退出前恢复默认控制台缓冲区
    shutdownConsoleDoubleBuffer();

    return 0;
}

// ==================== 功能实现 ====================

void initConsoleDoubleBuffer(void) {
    g_originalBuffer = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_originalBuffer == INVALID_HANDLE_VALUE || g_originalBuffer == NULL) return;

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
    }

    g_activeBuffer = 0;
    SetConsoleActiveScreenBuffer(g_buffers[g_activeBuffer]);
    g_doubleBufferEnabled = 1;
}

void shutdownConsoleDoubleBuffer(void) {
    if (!g_doubleBufferEnabled) return;

    SetConsoleActiveScreenBuffer(g_originalBuffer);

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
void draw(const Game *game) {
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
        int borderUnits = game->fullWidth ? WIDTH * 2 : WIDTH;

        printf("%s", leftTop);
        for (int i = 0; i < borderUnits; i++) printf("%s", hBorder);
        printf("%s  贪吃蛇游戏\n", rightTop);

        for (int y = 0; y < HEIGHT; y++) {
            printf("%s", vBorder);
            for (int x = 0; x < WIDTH; x++) {
                int isSnake = 0;

                if (x == game->snake.body[0].x && y == game->snake.body[0].y) {
                    printf("%s", headCh);
                    isSnake = 1;
                } else {
                    for (int i = 1; i < game->snake.length; i++) {
                        if (x == game->snake.body[i].x && y == game->snake.body[i].y) {
                            printf("%s", bodyCh);
                            isSnake = 1;
                            break;
                        }
                    }
                }

                if (!isSnake && x == game->food.x && y == game->food.y) {
                    printf("%s", foodCh);
                } else if (!isSnake) {
                    printf("%s", emptyCh);
                }
            }
            printf("%s", vBorder);

            if (y == 2) printf(u8"  分数: %d", game->score);
            if (y == 4) printf(u8"  长度: %d", game->snake.length);
            if (y == 6) printf(u8"  速度: %d", 150 - game->speed);
            if (y == 9) printf(u8"  控制: W/A/S/D");
            if (y == 10) printf(u8"  暂停: P");
            if (y == 11) printf(u8"  存档: K  读档: L");
            if (y == 12) printf(u8"  回车: 切换全/半角");
            if (y == 13) printf(u8"  当前: %s", modeText);
            if (y == 14) printf(u8"  状态: %s", stateText);
            if (y == 15) printf(u8"  退出: X");
            if (game->state == STATE_START && y == 17) printf(u8"  按任意键开始游戏");
            if (game->state == STATE_PAUSED && y == 17) printf(u8"  已暂停，按 P 继续");
            if (game->state == STATE_OVER && y == 17) printf(u8"  游戏结束，按 L 读档或 X 退出");

            printf("\n");
        }

        printf("%s", leftBottom);
        for (int i = 0; i < borderUnits; i++) printf("%s", hBorder);
        printf("%s\n", rightBottom);
        return;
    }

    int backBuffer = 1 - g_activeBuffer;
    HANDLE hBack = g_buffers[backBuffer];
    if (!hBack) return;

    wchar_t frame[8192];
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
    int borderUnits = game->fullWidth ? WIDTH * 2 : WIDTH;

    wchar_t headCh = game->fullWidth ? L'＠' : L'@';
    wchar_t bodyCh = game->fullWidth ? L'Ｏ' : L'o';
    wchar_t foodCh = game->fullWidth ? L'＊' : L'*';
    wchar_t emptyCh = game->fullWidth ? L'　' : L' ';

    // 顶部边框
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", leftTop);
    for (int i = 0; i < borderUnits; i++) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", hBorder);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  贪吃蛇游戏\n", rightTop);

    // 游戏区域
    for (int y = 0; y < HEIGHT; y++) {
        appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", vBorder);
        for (int x = 0; x < WIDTH; x++) {
            wchar_t cell = emptyCh;

            if (x == game->snake.body[0].x && y == game->snake.body[0].y) {
                cell = headCh;
            } else {
                for (int i = 1; i < game->snake.length; i++) {
                    if (x == game->snake.body[i].x && y == game->snake.body[i].y) {
                        cell = bodyCh;
                        break;
                    }
                }

                if (cell == emptyCh && x == game->food.x && y == game->food.y) {
                    cell = foodCh;
                }
            }

            appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%lc", cell);
        }

        appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", vBorder);

        if (y == 2) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  分数: %d", game->score);
        if (y == 4) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  长度: %d", game->snake.length);
        if (y == 6) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  速度: %d", 150 - game->speed);
        if (y == 9) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  控制: W/A/S/D");
        if (y == 10) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  暂停: P");
        if (y == 11) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  存档: K  读档: L");
        if (y == 12) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  回车: 切换全/半角");
        if (y == 13) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  当前: %ls", modeText);
        if (y == 14) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  状态: %ls", stateText);
        if (y == 15) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  退出: X");
        if (game->state == STATE_START && y == 17) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  按任意键开始游戏");
        if (game->state == STATE_PAUSED && y == 17) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  已暂停，按 P 继续");
        if (game->state == STATE_OVER && y == 17) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"  游戏结束，按 L 读档或 X 退出");

        appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"\n");
    }

    // 底部边框
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", leftBottom);
    for (int i = 0; i < borderUnits; i++) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", hBorder);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls\n", rightBottom);

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

// 键盘输入处理
void input(Game *game) {
    if (_kbhit()) {  // 检查是否有按键按下
        int key = _getch();  // 获取按键（不回显）

        // 扩展按键（方向键等）读取第二字节，仍可触发开局
        if (key == 0 || key == 224) {
            if (_kbhit()) {
                _getch();
            }
            if (game->state == STATE_START) game->state = STATE_RUNNING;
            return;
        }

        // 回车切换全角/半角模式
        if (key == '\r') {
            game->fullWidth = !game->fullWidth;
            if (game->state == STATE_START) game->state = STATE_RUNNING;
            return;
        }

        if (key == 'k' || key == 'K') {
            saveGame(game);
            return;
        }

        if (key == 'l' || key == 'L') {
            loadGame(game);
            return;
        }

        if (key == 'p' || key == 'P') {
            if (game->state == STATE_RUNNING) {
                game->state = STATE_PAUSED;
            } else if (game->state == STATE_PAUSED || game->state == STATE_START) {
                game->state = STATE_RUNNING;
            }
            return;
        }

        if (key == 'x' || key == 'X') {
            game->gameOver = 1;
            return;
        }

        // 首次任意键开局
        if (game->state == STATE_START) {
            game->state = STATE_RUNNING;
            return;
        }

        if (game->state != STATE_RUNNING) return;
        
        // 防止180度转向（不能直接反向）
        switch (key) {
            case 'w':
            case 'W':
                if (game->snake.dir != DOWN) game->snake.dir = UP;
                break;
            case 's':
            case 'S':
                if (game->snake.dir != UP) game->snake.dir = DOWN;
                break;
            case 'a':
            case 'A':
                if (game->snake.dir != RIGHT) game->snake.dir = LEFT;
                break;
            case 'd':
            case 'D':
                if (game->snake.dir != LEFT) game->snake.dir = RIGHT;
                break;
        }
    }
}

// 游戏逻辑更新
void logic(Game *game) {
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
        game->state = STATE_OVER;
        return;
    }
    
    // 撞自己检测（从1开始，不检查蛇头自身）
    for (int i = 1; i < game->snake.length; i++) {
        if (newHead.x == game->snake.body[i].x && 
            newHead.y == game->snake.body[i].y) {
            game->state = STATE_OVER;
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
