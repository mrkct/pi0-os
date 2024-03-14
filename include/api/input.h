#ifndef API_INPUT_H
#define API_INPUT_H

#include <stdint.h>

typedef struct {
    char character;
    uint32_t keycode;
    bool press_state;
} KeyEvent;

#endif