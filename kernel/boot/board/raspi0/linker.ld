ENTRY(_start)


SECTIONS
{
    . = 0x8000;

    .text :
    {
        KEEP(*(.text.entry))
        KEEP(*(.text))
    }

    .rodata :
    {
        KEEP(*(.rodata))
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

    
    .bundle :
    {
        . = ALIGN(4K);
        __bundle_start = .;
        KEEP(*(.bundle))
        __bundle_end = .;
    }
    
}
