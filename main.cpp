#include "SnakeGame.h"

int main() {
    Game game;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    initConsoleDoubleBuffer();
    initRandomSeed();
    initGame(&game);
    runGameLoop(&game);
    shutdownConsoleDoubleBuffer();

    return 0;
}
