
global page_table_l4
global page_table_l3
global page_table_l2
global stack_bottom
global stack_top
global idt_table
global idtr
global gdt64
global gdt64_pointer
global gdt64_end

; =====================================================================
; 7. VERİ ALANLARI (GDT, IDT TABLOSU VE STACK)
; =====================================================================
section .bss
align 4096
page_table_l4: resb 4096
page_table_l3: resb 4096
page_table_l2: resb 4096
stack_bottom:  resb 4096 * 4
stack_top:
align 16
idt_table:     resb 256 * 16

section .data
align 16
idtr:
    dw (256 * 16) - 1
    dq idt_table

section .rodata
align 8
gdt64:
    dq 0
    dq 0x00af9a000000ffff ; 64-bit Code Segment
    dq 0x00af92000000ffff ; 64-bit Data Segment
gdt64_pointer:
    dw gdt64_end - gdt64 - 1
    dq gdt64
gdt64_end:
