# pi0-os

A fully from scratch operating system the QEMU ARM "virt" machine
and the Raspberry Pi Zero.
Still need to find a good name for it.

There's barely a GUI or any usable software, but you can run DOOM:
![DOOM](docs/pics/qemu-doom.png)


## How to try it out

The easiest way to try it out is to install `qemu-system-arm`, download a package
from the CI pipeline and run the script inside it.
That will start a QEMU instance setup with peripherals and a disk image with all
the available programs for you to play with.

## Build instructions

There is a Docker image that can be used to build the project.
You cannot use it to run QEMU though, so you'll have to install `qemu-system-arm`
separately outside of it, and also `make`.

To build using Docker, run the following commands:

```
docker build -t pi0-os .
docker run -v $(pwd):/pi0-os pi0-os
```

This will build the kernel, all of the userland programs and automatically
create a disk image.

Running `make qemu` will start the QEMU instance using the built kernel
and disk image.

## Debugging and stuff

Here's a useful `.gdbinit`, add it to your `kernel/` directory.

```
target remote localhost:1234
file kernel.elf
add-symbol-file boot/boot.elf
```

`make qemu-gdb` will start QEMU with the GDB stub enabled and paused.
On another terminal, run `gdb-multiarch` inside the `kernel/` directory
and it will automatically attach to the running QEMU instance and load
the symbols file for both the kernel and the bootloader.

### Reverse debugging

`RECORD=1 make qemu` will record the execution of QEMU.
`REPLAY=1 make qemu-gdb` will replay the recorded execution
