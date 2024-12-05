#ifndef ARM_CRT0UTIL_H
#define ARM_CRT0UTIL_H

#include <stdint.h>

typedef struct ArmCrt0InitialStackState {
    uint32_t argc;
    char **argv;
    uint32_t envc;
    char **envp;
} ArmCrt0InitialStackState;

#endif
