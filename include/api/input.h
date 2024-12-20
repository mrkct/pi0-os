#ifndef API_INPUT_H
#define API_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
namespace api {
#endif

#include "input-event-codes.h"


struct InputEvent {
    uint16_t type;
    union {
        struct {
            uint16_t keycode;
            uint16_t modifiers;
        } key;
        struct {
            uint16_t abs_x;
            uint16_t abs_y;
        } mouse;
    };
};

typedef struct InputEvent InputEvent;

#ifdef __cplusplus
}
#endif

#endif