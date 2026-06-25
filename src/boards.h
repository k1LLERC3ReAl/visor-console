#ifndef BOARDS_H
#define BOARDS_H

typedef struct {
    char name[48];
    char fqbn[64];
} BoardDef;

extern const BoardDef KNOWN_BOARDS[];
extern const int KNOWN_BOARD_COUNT;

#endif
