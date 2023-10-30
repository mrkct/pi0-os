#!/bin/sh

find -name "*.cpp" -not -path "./userland/doomgeneric/*" -o \
     -name "*.c" -not -path "./userland/doomgeneric/*" -o \
     -name "*.h" -not -path "./userland/doomgeneric/*" -exec clang-format-17 -i {} \;
