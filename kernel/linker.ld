ENTRY(_start)
 
SECTIONS
{
    . = 0x0000;
    .vector_table :
    {
        KEEP(*(.vector_table.table))
    }
    .vector_table.trampolines ALIGN(4K) :
    {
        KEEP(*(.vector_table.trampolines))
    }

    . = 0x8000;
    __prekernel_start = .;
    .prekernel.text ALIGN(4K) :
    {
        KEEP(*(.prekernel.text))
    }
    __prekernel_end = .;

    . = 0xe0000000 + 1M;
    __kernel_start = .;
    
    .text ALIGN(4K) : AT(ADDR(.text) - 0xe0000000)
    {
        . = ALIGN(4);
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
    }
    __bss_size = __bss_end - __bss_start;
    
    . = ALIGN(4096);
    __kernel_end = .;
    __heap_start = .;
}
