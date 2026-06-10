OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot2 : ALIGN(8) {
        KEEP(*(.multiboot2))
    }

    . = ALIGN(4K);
    _text_start = .;
    .text : {
        *(.text .text.*)
    }
    _text_end = .;

    . = ALIGN(4K);
    _rodata_start = .;
    .rodata : {
        *(.rodata .rodata.*)
    }
    _rodata_end = .;

    . = ALIGN(4K);
    _data_start = .;
    .data : {
        *(.data .data.*)
    }

    . = ALIGN(8);
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    }
    . = ALIGN(4K);
    _data_end = .;

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
        *(.comment*)
    }
}
