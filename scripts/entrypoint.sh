#!/bin/sh

printf "\033[1;32mBuilding kernel...\n\033[0m"
make -j$(nproc) kernel

printf "\033[1;32mBuilding userland...\n\033[0m"
make -j$(nproc) userland
