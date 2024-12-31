#!/bin/sh

printf "\033[1;32mBuilding kernel...\n\033[0m"
make kernel

printf "\033[1;32mBuilding userland...\n\033[0m"
make userland
