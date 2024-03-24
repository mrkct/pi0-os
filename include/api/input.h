#ifndef API_INPUT_H
#define API_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
namespace api {
#endif

typedef struct {
    char character;
    uint32_t keycode;
    bool press_state;
} KeyEvent;

#ifdef __cplusplus
}
#endif

#endif