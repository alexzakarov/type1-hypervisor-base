ENTRY(_start)
SECTIONS
{
    . = 0x100000; /* 1MB Başlangıç */

    .text : { *(.multiboot_header) *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }

    . = ALIGN(4096);
    end = .; /* Kernel kodlarının bittiği tam adres sembolü */
}
