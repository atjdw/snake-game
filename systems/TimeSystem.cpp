#include "../SnakeGame.h"

#include <stdlib.h>
#include <time.h>

void initRandomSeed(void) {
    srand((unsigned)time(NULL));
}

void sleepFrame(int milliseconds) {
    Sleep(milliseconds);
}
