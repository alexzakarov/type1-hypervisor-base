; =====================================================================
; 1. MULTIBOOT BAŞLIĞI (GRUB/QEMU Tarafından Tanınması İçin)
; =====================================================================
section .multiboot_header
align 4
MBOOT_MAGIC      equ 0x1BADB002
MBOOT_FLAGS      equ (1 << 0) | (1 << 1)
MBOOT_CHECKSUM   equ -(MBOOT_MAGIC + MBOOT_FLAGS)

    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM
    times 5 dd 0

; =====================================================================
; 2. 32-BİT BAŞLANGIÇ VE SAYFALAMA (PAGING) KURULUMU
; =====================================================================
bits 32
section .text
global _start
extern kmain
extern asm_init_pic, asm_init_idt, stack_top, gdt64_pointer, page_table_l2, page_table_l3, page_table_l4

_start:
    ; KRİTİK DÜZELTME 1: GRUB'ın getirdiği Multiboot adresini (EBX)
    ; alttaki sayfalama fonksiyonları ezmeden önce hemen yığına (stack) itip koruyoruz.
    push ebx

    ; Geçici 32-bit yığın (stack) kurulumu
    mov esp, stack_top

    ; 64-bit mod için Sayfa Tablolarını (Page Tables) hazırla
    call setup_page_tables
    call enable_paging

    ; 64-bit GDT yükle ve Uzun Moda (Long Mode) kesin geçiş yap
    lgdt [gdt64_pointer]
    jmp 0x08:long_mode_init

setup_page_tables:
    mov eax, page_table_l3
    or eax, 0b11 ; PRESENT | WRITABLE
    mov [page_table_l4], eax

    mov eax, page_table_l2
    or eax, 0b11
    mov [page_table_l3], eax

    mov ecx, 0
.loop:
    mov eax, 0x200000 ; 2MB Sayfa boyutu
    mul ecx
    or eax, 0b10000011 ; 2MB Page, PRESENT, WRITABLE
    mov [page_table_l2 + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .loop
    ret

enable_paging:
    mov eax, page_table_l4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5 ; PAE (Physical Address Extension) aktif
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr          ; DİKKAT: Bu komut EAX ve EDX register'larını ezer!
    or eax, 1 << 8 ; Long Mode aktif
    wrmsr
    mov eax, cr0
    or eax, 1 << 31 ; Paging (Sayfalama) aktif
    mov cr0, eax
    ret

; =====================================================================
; 3. 64-BİT UZUN MOD (LONG MODE) BAŞLANGICI
; =====================================================================
[bits 64]
long_mode_init:
    ; Segment kayıtçılarını (registers) güncelle
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; KRİTİK DÜZELTME 2: _start başında yığına sakladığımız orijinal adresi
    ; 64-bit modda yığından RBX olarak geri çekiyoruz.
    pop rbx

    ; 64-bit System V AMD64 ABI çağırma kuralına göre ilk parametre RDI olmalıdır.
    ; RBX içindeki temizlenmiş orijinal adresi RDI (EDI) register'ına güvenle geçiriyoruz.
    mov edi, ebx

    ; Donanım Kesme Denetleyicisini (PIC) yeniden haritalandır
    call asm_init_pic

    ; IDT Tablosunu doldur ve işlemciye yükle
    call asm_init_idt

    ; İşlemci seviyesinde kesmeleri aktif et
    sti

    ; C++ ana fonksiyonunu çağır (EDI içindeki adres kmain'e parametre gider)
    call kmain

.halt_loop:
    hlt
    jmp .halt_loop


[bits 64]
global asm_execute_vmxon

; RDI = C++ tarafından gönderilen VMXON bellek bölgesinin fiziksel adresi
asm_execute_vmxon:
    ; VMXON komutu parametresini hafıza adresi (pointer) olarak yığından ister
    push rdi                ; Adresi stack'e koy
    vmxon [rsp]             ; VMX modunu bu bellek adresiyle başlat!
    pop rdi                 ; Stack'i temizle

    ; VMXON komutunun başarı durumunu kontrol et (RFLAGS register'ı üzerinden)
    ; JZ (Zero Flag) = Başarısız (VMXON pointer hatası)
    ; JC (Carry Flag) = Başarısız (Donanım hatası)
    jc .error
    jz .error

    mov rax, 1              ; Başarılı ise 1 dön
    ret

.error:
    mov rax, 0              ; Başarısız ise 0 dön
    ret


            ; VMRESUME başarısız olursa işlemciyi kilitle