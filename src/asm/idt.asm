global asm_init_pic
global asm_init_idt

extern idt_table, idtr

; =====================================================================
; 4. ASSEMBLY PIC YAPILANDIRMASI (8259 PIC)
; =====================================================================
asm_init_pic:
    ; ICW1: Başlatma komutu
    mov al, 0x11
    out 0x20, al
    out 0xA0, al

    ; ICW2: Donanım kesmelerini IDT 32 (Master) ve IDT 40 (Slave)'a haritala
    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al

    ; ICW3: Master/Slave pin bağlantıları
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al

    ; ICW4: 8086 modunu etkinleştir
    mov al, 0x01
    out 0x21, al
    out 0xA1, al

    ; Kesme Maskeleri: Sadece IRQ0 (Timer) ve IRQ1 (Klavye) açık
    mov al, 0xFC ; 11111100b (0 = Açık)
    out 0x21, al
    mov al, 0xFF ; Slave tamamen kapalı
    out 0xA1, al
    ret

; =====================================================================
; 5. ASSEMBLY IDT KURULUMU
; =====================================================================
asm_init_idt:
    ; IRQ0 (Timer) -> Vektör 32'ye bağla
    mov rsi, irq0_handler_stub
    mov rdi, 32
    call asm_set_idt_entry

    ; IRQ1 (Klavye) -> Vektör 33'e bağla
    mov rsi, irq1_handler_stub
    mov rdi, 33
    call asm_set_idt_entry

    ; IDTR kaydını işlemciye yükle
    lidt [idtr]
    ret

; RSI = Handler Fonksiyon Adresi, RDI = Vektör Numarası
asm_set_idt_entry:
    shl rdi, 4 ; RDI = RDI * 16 (64-bit IDT'de her giriş 16 byte'tır)
    add rdi, idt_table

    ; Offset Low (0-15. bitler)
    mov ax, si
    mov [rdi], ax

    ; Selector (GDT Code)
    mov word [rdi + 2], 0x08

    ; IST & Attributes (0x8E = 64-bit Interrupt Gate, Present, Ring 0)
    mov word [rdi + 4], 0x8E00

    ; Offset Mid (16-31. bitler)
    shr rsi, 16
    mov [rdi + 6], si

    ; Offset High (32-63. bitler)
    shr rsi, 16
    mov [rdi + 8], esi

    ; Reserved (Ayrılmış alan)
    mov dword [rdi + 12], 0
    ret

; =====================================================================
; 6. KESME KÖPRÜLERİ (ISR STUBS)
; =====================================================================
%macro push_all_registers 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro pop_all_registers 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

global irq0_handler_stub
extern c_irq0_handler

irq0_handler_stub:
    push_all_registers
    call c_irq0_handler ; C++ fonksiyonunu çağır

    ; PIC'e EOI sinyali gönder
    mov al, 0x20
    out 0x20, al

    pop_all_registers
    iretq

global irq1_handler_stub
extern c_irq1_handler

irq1_handler_stub:
    push_all_registers
    call c_irq1_handler ; C++ fonksiyonunu çağır

    ; PIC'e EOI sinyali gönder
    mov al, 0x20
    out 0x20, al

    pop_all_registers
    iretq
