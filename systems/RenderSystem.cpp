#include "../SnakeGame.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static HANDLE g_originalBuffer = NULL;
static HANDLE g_buffers[2] = {NULL, NULL};
static int g_activeBuffer = 0;
static int g_doubleBufferEnabled = 0;
static COORD g_bufferSize = {0, 0};
static DWORD g_originalConsoleMode = 0;

#define ANSI_RESET L"\x1b[0m"
#define ANSI_BOLD L"\x1b[1m"
#define ANSI_DIM L"\x1b[2m"
#define ANSI_WHITE L"\x1b[37m"
#define ANSI_GRAY L"\x1b[90m"
#define ANSI_RED L"\x1b[91m"
#define ANSI_GREEN L"\x1b[92m"
#define ANSI_YELLOW L"\x1b[93m"
#define ANSI_BLUE L"\x1b[94m"
#define ANSI_CYAN L"\x1b[96m"

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
            NULL);
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

    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls", ANSI_BOLD, ANSI_CYAN);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", leftTop);
    for (int i = 0; i < borderUnits; i++) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", hBorder);
    appendf(frame,
            sizeof(frame) / sizeof(frame[0]),
            &offset,
            L"%ls%ls  贪吃蛇游戏%ls\n",
            rightTop,
            ANSI_RESET,
            ANSI_RESET);

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
                        appendf(frame,
                                sizeof(frame) / sizeof(frame[0]),
                                &offset,
                                L"%ls%lc%ls",
                                ANSI_GREEN,
                                bodyCh,
                                ANSI_BLUE);
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
        if (y == 8) appendf(frame,
                            sizeof(frame) / sizeof(frame[0]),
                            &offset,
                            L"%ls  当前难度: %d级%ls",
                            ANSI_YELLOW,
                            getDifficultyLevel(game->speed),
                            ANSI_RESET);
        if (y == 9) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  控制模式: %ls%ls", ANSI_CYAN, controllerText, ANSI_RESET);
        if (y == 10) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  方向: W/A/S/D%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 11) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  切模式: M%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 12) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  暂停: P%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 13) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  存档: K  读档: L%ls", ANSI_GRAY, ANSI_RESET);
        if (y == 14) appendf(frame,
                             sizeof(frame) / sizeof(frame[0]),
                             &offset,
                             L"%ls  回车: 切换全/半角%ls",
                             ANSI_GRAY,
                             ANSI_RESET);
        if (y == 15) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  当前: %ls%ls", ANSI_WHITE, modeText, ANSI_RESET);
        if (y == 16) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  状态: %ls%ls", ANSI_WHITE, stateText, ANSI_RESET);
        if (y == 17) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls  退出: X%ls", ANSI_RED, ANSI_RESET);
        if (game->state == STATE_START && y == 18) appendf(frame,
                                                             sizeof(frame) / sizeof(frame[0]),
                                                             &offset,
                                                             L"%ls  按任意键开始游戏%ls",
                                                             ANSI_GREEN,
                                                             ANSI_RESET);
        if (game->state == STATE_PAUSED && y == 18) appendf(frame,
                                                              sizeof(frame) / sizeof(frame[0]),
                                                              &offset,
                                                              L"%ls  已暂停，按 P 继续%ls",
                                                              ANSI_YELLOW,
                                                              ANSI_RESET);
        if (game->state == STATE_OVER && y == 18) appendf(frame,
                                                           sizeof(frame) / sizeof(frame[0]),
                                                           &offset,
                                                           L"%ls  游戏结束，按 L 读档或 X 退出%ls",
                                                           ANSI_RED,
                                                           ANSI_RESET);

        appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"\n");
    }

    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls", ANSI_BOLD, ANSI_CYAN);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", leftBottom);
    for (int i = 0; i < borderUnits; i++) appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls", hBorder);
    appendf(frame, sizeof(frame) / sizeof(frame[0]), &offset, L"%ls%ls\n", rightBottom, ANSI_RESET);

    DWORD written = 0;
    COORD origin = {0, 0};
    FillConsoleOutputCharacterW(hBack, L' ', (DWORD)(g_bufferSize.X * g_bufferSize.Y), origin, &written);
    SetConsoleCursorPosition(hBack, origin);
    WriteConsoleW(hBack, frame, (DWORD)wcslen(frame), &written, NULL);

    SetConsoleActiveScreenBuffer(hBack);
    g_activeBuffer = backBuffer;
}
