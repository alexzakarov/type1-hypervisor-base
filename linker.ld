ENTRY(_start)
SECTIONS
{
    . = 0x00100000;

    .boot : {
        KEEP(*(.multiboot_header))
    }


    .text : {
        *(.text)
    }


    _kernel_end = .;
}