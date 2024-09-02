# pi0-os

A fully from scratch operating system for the Raspberry Pi Zero.
Still need to find a good name for it.

**Build instructions:**
1. Install `arm-none-eabi-gcc` and `qemu-system-arm`
2. Run `make qemu`

I am working on a large refactor to add support for more boards,
especially the QEMU virt machine (still ARM) and mostly to move away from
ARMv6 into ARMv7.

There's barely a GUI or any usable software, but you can run DOOM:
![DOOM](docs/pics/qemu-doom.png)
