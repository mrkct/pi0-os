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
    .prekernel.data ALIGN(4K):
    {
        *(.prekernel.data)
    }
    .prekernel.data.early_translation_table ALIGN(16K):
    {
        *(.prekernel.data.early_translation_table)
    }
    __prekernel_end = .;

    . = 0xe0000000 + 1M;
    __kernel_start = .;
    __text_start = .;
    .text ALIGN(4K) : AT(ADDR(.text) - 0xe0000000)
    {
        *(.text)
    }
    __text_end = .;
    __rodata_start = .;
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xe0000000)
    {
        *(.rodata)
    }
    __rodata_end = .;
    __data_start = .;
    .data ALIGN(4K) : AT(ADDR(.data) - 0xe0000000)
    {
        *(.data)
    }
    __data_end = .;
    __bss_start = .;
    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xe0000000)
    {
        bss = .;
        *(.bss)
    }
    __bss_end = .;
    __bss_size = __bss_end - __bss_start;
    
    . = ALIGN(4096);
    __kernel_end = .;
    __heap_start = .;
}
