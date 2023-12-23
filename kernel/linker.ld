ENTRY(_start)

SECTIONS
{
    .text.start :
    {
        . = 0x8000;
        KEEP(*(.text.start.boot))
        KEEP(*(.text.start))
    }

    .rodata.start :
    {
        KEEP(*(.rodata.start))
    }

    .text (0xe0100000) : AT(ADDR(.text) - 0xe0000000)
    {
        __higher_half_start = .;
        __kernel_start = .;
        __text_start = .;

        *(.text)

        __text_end = .;
        . = ALIGN(4);
    }
    
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xe0000000)
    {
        . = ALIGN(4);
        __rodata_start = .;

        *(.rodata)
        *(.rodata*)

        . = ALIGN(4);
        __rodata_end = .;
    }
    
    .data ALIGN(4K) : AT(ADDR(.data) - 0xe0000000)
    {
        . = ALIGN(4);
        __data_start = .;

        *(.data)

        . = ALIGN(4);
        __data_end = .;
    }
    
    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xe0000000)
    {
        . = ALIGN(4);
        __bss_start = .;

        bss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)

        . = ALIGN(4);
        __bss_end = .;
        __bss_size = __bss_end - __bss_start;
    }
    
    /* This alignment is required due to the current implementation of the physical page allocator */
    . = ALIGN(1M);

    __kernel_end = .;
    __heap_start = .;
}
