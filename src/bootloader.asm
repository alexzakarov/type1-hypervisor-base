section .multiboot

MBOOT_PAGE_ALIGN EQU 1 << 0
MBOOT_MEM_INFO   EQU 1 << 1
MBOOT_USE_GFX    EQU 0

MBOOT_MAGIC      EQU 0x1BADB002
MBOOT_FLAGS      EQU MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_USE_GFX
MBOOT_CHECKSUM   EQU -(MBOOT_MAGIC + MBOOT_FLAGS)

PAGE_PRESENT EQU 1 << 0
PAGE_WRITABLE EQU 1 << 1
PAGE_SIZE EQU 1 << 7



section .multiboot_header
ALIGN 4
    DD MBOOT_MAGIC
    DD MBOOT_FLAGS
    DD MBOOT_CHECKSUM
    DD 0, 0, 0, 0, 0

    DD 0
    DD 800
    DD 600
    DD 32

bits 32

section .text
global start

extern kmain

extern long_mode_start

start:
    mov esp, stack_top

    call setup_page_tables
    call enable_paging


    lgdt [gdt64_pointer]

    jmp 0x08:long_mode_start

    hlt


setup_page_tables:
    mov eax, page_table_l3
    or eax, PAGE_PRESENT | PAGE_WRITABLE
    mov [page_table_l4], eax

    mov eax, page_table_l2
    or eax, PAGE_PRESENT | PAGE_WRITABLE
    mov [page_table_l3], eax

    mov ecx, 0

.loop:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [page_table_l2 + ecx * 8], eax

    inc ecx
    cmp ecx, 512
    jne .loop

    ret

enable_paging:
    mov eax, page_table_l4
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    ret

section .bss
align 4096
page_table_l4:
    resb 4096
page_table_l3:
    resb 4096
page_table_l2:
    resb 4096
stack_bottom:
    resb 4096 * 4
stack_top:

section .rodata
align 8
gdt64:
    dq 0
    dq 0x00af9a000000ffff
    dq 0x00af92000000ffff
gdt64_pointer:
    dw gdt64_end -  gdt64 -1
    dq gdt64

gdt64_end:
align 4

header_start:
    dd 0x1BADB002
    dd 0x00000000
    dd -(0x1BADB002)

    align 8
header_end:
