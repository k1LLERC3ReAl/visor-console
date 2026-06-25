#include "boards.h"

const BoardDef KNOWN_BOARDS[] = {
    { "Arduino Nano (old bootloader)", "arduino:avr:nano:cpu=atmega328old" },
    { "Arduino Nano (new bootloader)", "arduino:avr:nano:cpu=atmega328p" },
    { "Arduino Uno", "arduino:avr:uno" },
    { "Arduino Mega 2560", "arduino:avr:mega:cpu=atmega2560" },
};

const int KNOWN_BOARD_COUNT = sizeof(KNOWN_BOARDS) / sizeof(KNOWN_BOARDS[0]);
