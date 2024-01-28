#include <kernel/kprintf.h>
#include <kernel/device/ramdisk.h>
#include <kernel/lib/libc/string.h>


namespace kernel {

static constexpr uint32_t BLOCK_SIZE = 512;

#ifdef RAMDISK_FILE

#define __STRINGIFY(s) #s
#define STRINGIFY(s) __STRINGIFY(s)

asm(
".section .ramdisk, \"ao\" \n"
".global __start_of_ramdisk \n"
"__start_of_ramdisk: \n"
    ".incbin \"" STRINGIFY(RAMDISK_FILE) "\" \n"
".global __end_of_ramdisk \n"
"__end_of_ramdisk:"
);

extern "C" uint8_t __start_of_ramdisk[];
extern "C" uint8_t __end_of_ramdisk[];

#else

uint8_t *__start_of_ramdisk = nullptr;
uint8_t *__end_of_ramdisk = nullptr;

#endif

#pragma GCC diagnostic ignored "-Waddress"
bool ramdisk_probe()
{
    return __start_of_ramdisk != nullptr;
}

Error ramdisk_init(Storage &storage)
{
    if (!ramdisk_probe())
        return DeviceNotConnected;

    storage = Storage{
        .read_block = [](auto&, uint64_t block_idx, uint8_t *out_buffer) {
            memcpy(out_buffer, &__start_of_ramdisk[block_idx * BLOCK_SIZE], BLOCK_SIZE);
            return Success;
        },
        .write_block = [](auto&, uint64_t block_idx, uint8_t const* buffer) {
            memcpy(&__start_of_ramdisk[block_idx * BLOCK_SIZE], buffer, BLOCK_SIZE);
            return Success;
        },
        .get_block_count = [](auto&, uint64_t& out_block_count) {
            out_block_count = reinterpret_cast<uintptr_t>(__end_of_ramdisk) - reinterpret_cast<uintptr_t>(__start_of_ramdisk) / BLOCK_SIZE;
            return Success;
        },
        .block_idx_offset = 0,
        .data = nullptr
    };
    return Success;
}

}