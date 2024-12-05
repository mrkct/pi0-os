ENTRY(_start)


SECTIONS
{
    . = 0xc0000000;

    .text :
    {
        __text_start = .;
        KEEP(*(.text.entry))
        KEEP(*(.text))
        __text_end = .;
    }

    .rodata :
    {
        __rodata_start = .;
        KEEP(*(.rodata))
        __rodata_end = .;
    }

    .data :
    {
        __data_start = .;
        KEEP(*(.data))
        __data_end = .;
    }

    .bss :
    {
        __bss_start = .;
        KEEP(*(.bss))
        . = ALIGN(64);
        __bss_end = .;
    }

    /*
        This extra section at the end is important!

        The bootloader doesn't include an ELF loader, it just loads the .bin
        created with objcopy. The bootloader must also map memory for the
        .bss, but objcopy rightfully strips it away if its possible.
        
        This dummy value at the end forces objcopy to keep the .bss.
    */
    .bootloaderhack :
    {
        LONG(1234);
    }
}
