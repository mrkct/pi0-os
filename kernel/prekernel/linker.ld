ENTRY(_start)
 
SECTIONS
{
    . = 0x0000;
    .vector_table :
    {
        KEEP(*(.vector_table.table))
    }

    . = 0x8000;
    __start = .;
    __text_start = .;
    .text :
    {
        KEEP(*(.text.boot))
        KEEP(*(.text.vector_trampolines))
        *(.text)
    }
    . = ALIGN(4096);
    __text_end = .;
 
    __rodata_start = .;
    .rodata :
    {
        *(.rodata)
    }
    . = ALIGN(4096);
    __rodata_end = .;
 
    __data_start = .;
    .data :
    {
        *(.data)
        KEEP(*(.data.boot))
        . = ALIGN(16384);
        KEEP(*(.data.boot.early_translation_table))
    }
    . = ALIGN(4096);
    __data_end = .;
 
    __bss_start = .;
    .bss :
    {
        bss = .;
        *(.bss)
    }
    . = ALIGN(4096);
    __bss_end = .;
    __bss_size = __bss_end - __bss_start;

    . = ALIGN(4096);
    __heap_start = .;


    __end = .;
}
