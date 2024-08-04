ENTRY(_start)


SECTIONS
{
    . = LOAD_ADDRESS;

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
        __bundle_kernel_start = .;
        KEEP(*(.bundle.kernel))
        __bundle_kernel_end = .;
        
        . = ALIGN(4K);
        __bundle_dtb_start = .;
        KEEP(*(.bundle.dtb));
        __bundle_dtb_end = .;
    }
}
