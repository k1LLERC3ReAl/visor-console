#include "utils.h"
#include <windows.h>

uint64_t utils_millis(void) {
    return (uint64_t)GetTickCount64();
}
