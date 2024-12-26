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
            uint16_t code;
            uint32_t value;
        } raw;

        struct {
            uint16_t code;
            uint32_t pressed;
        } key;
    };
};

typedef struct InputEvent InputEvent;

#ifdef __cplusplus
}
#endif

#endif