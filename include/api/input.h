#ifndef API_INPUT_H
#define API_INPUT_H

#include <stdint.h>

typedef struct {
    char character;
    uint32_t keycode;
    bool press_state;
} KeyEvent;

static inline int poll_keyboard_event(KeyEvent *event)
{
    return syscall(SYS_PollInput, NULL, 0, (uint32_t) event, 0, 0, 0);
}

#endif