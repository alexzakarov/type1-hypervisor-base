[bits 64]
global asm_launch_intel_vm
global asm_launch_amd_vm

extern c_vmexit_handler
extern slat_pml4_phys
extern simulated_guest_kernel
extern guest_gdt_phys
extern guest_idt_phys

; =====================================================================
; AMD VMRUN LOOP (MUTLAK DONANIM VE TEMİZLEME GARANTİLİ)
; =====================================================================
asm_launch_amd_vm:
    push rbp
    mov rbp, rsp

    ; C++'dan gelen orijinal VMCB fiziksel adresini (RDI) RAX'e alıyoruz
    mov rax, rdi

    ; -----------------------------------------------------------------
    ; KRİTİK GÜVENCE: VMCB KONTROL ALANINI (İLK 1024 BYTE) SIFIRLA
    ; -----------------------------------------------------------------
    ; Eski çöp verilerin VMRUN kural doğrulamalarını (Consistency Checks)
    ; baltalamasını engellemek için ilk 1024 byte'ı (256 adet 4-byte) sıfırlıyoruz.
    mov rcx, 256
.vmcb_clear_loop:
    mov dword [rax + rcx * 4 - 4], 0
    loop .vmcb_clear_loop

    ; --------------------------------=================================
    ; ASSEMBLY SEVİYESİNDE MİLİMETRİK VMCB DOLGUSU (Sıfır Hata Payı)
    ; --------------------------------=================================

    ; --- A. KONTROL ALANLARI (Ofset 0x000 - 0x3FF, Linux svm.h layout) ---
    ; intercepts[0..5] = 6 x u32 (CR/DR/EXC/WORD3/WORD4/WORD5)
    ; Bit konumlari Linux svm.h enum'lari ile uyumlu.
    mov dword [rax + 0x000], 0x00080008 ; intercepts[0]=CR: CR3 read(bit3) + CR3 write(bit19)
    mov dword [rax + 0x008], 0x00004000 ; intercepts[2]=EXCEPTION: #PF (bit14)
    mov dword [rax + 0x00C], 0x01000000 ; intercepts[3]=WORD3: HLT (bit24)
    mov dword [rax + 0x010], 0x00000007 ; intercepts[4]=WORD4: VMRUN+VMMCALL+VMLOAD
    mov dword [rax + 0x058], 1           ; asid = 1 (Zorunlu, sifirdan buyuk)
    mov qword [rax + 0x090], 1           ; nested_ctl: SVM_NPT_ENABLED (bit 0) - Nested Paging aktif

    mov rbx, [slat_pml4_phys]
    mov [rax + 0x0B0], rbx              ; nested_cr3 (u64 @0xB0)

    ; --- B. GUEST-STATE DURUM ALANLARI (Ofset 0x400 - 0xFFF, Linux svm.h save area) ---
    ; struct vmcb_save_area: cr4@0x148, cr3@0x150, cr0@0x158,
    ; rflags@0x170, rip@0x178, rsp@0x1D8, efer@0xD0
    ; VMCB ofset = 0x400 + save_area_ofseti
    mov qword [rax + 0x558], 0x00000011 ; guest_cr0 (PE=1, ET=1, PG=0)
    mov qword [rax + 0x550], 0          ; guest_cr3 (sayfalama kapali, 0)
    mov qword [rax + 0x548], 0          ; guest_cr4
    mov qword [rax + 0x570], 0x202      ; guest_rflags (bit1 reserved-1, IF=1)

    mov rbx, simulated_guest_kernel
    mov [rax + 0x578], rbx              ; guest_rip = simulated_guest_kernel

    mov rbx, rsp
    sub rbx, 8192
    mov [rax + 0x5D8], rbx              ; guest_rsp
    mov qword [rax + 0x4D0], 0x1000       ; guest_efer: SVME bit (bit 12) ZORUNLU - SVM aktif

    ; --- C. SEGMENT NITELIKLERI (struct vmcb_seg = u16 sel/u16 attr/u32 limit/u64 base, 16 byte) ---
    ; Save area: es@0x400, cs@0x410, ss@0x420, ds@0x430,
    ;            fs@0x440, gs@0x450, gdtr@0x460, ldtr@0x470, idtr@0x480, tr@0x490

    ; Guest ES Segment (Ofset 0x400)
    mov word [rax + 0x400], 0x10
    mov word [rax + 0x402], 0x0C93      ; 32-bit Data: D/B=1(bit10), G=1(bit11), P=1, S=1, RW
    mov dword [rax + 0x404], 0xFFFFFFFF
    mov qword [rax + 0x408], 0

    ; Guest CS Segment (Ofset 0x410)
    mov word [rax + 0x410], 0x08
    mov word [rax + 0x412], 0x0C9B      ; 32-bit Code: D/B=1(bit10), G=1(bit11), P=1, S=1, Exec/Read
    mov dword [rax + 0x414], 0xFFFFFFFF
    mov qword [rax + 0x418], 0

    ; Guest SS Segment (Ofset 0x420)
    mov word [rax + 0x420], 0x10
    mov word [rax + 0x422], 0x0C93      ; 32-bit Data: D/B=1(bit10), G=1(bit11), P=1, S=1, RW
    mov dword [rax + 0x424], 0xFFFFFFFF
    mov qword [rax + 0x428], 0

    ; Guest DS Segment (Ofset 0x430)
    mov word [rax + 0x430], 0x10
    mov word [rax + 0x432], 0x0C93      ; 32-bit Data: D/B=1(bit10), G=1(bit11), P=1, S=1, RW
    mov dword [rax + 0x434], 0xFFFFFFFF
    mov qword [rax + 0x438], 0

    ; Guest TR Segment (Ofset 0x490) - 32-bit busy TSS, sunumun surekliligi icin
    mov word [rax + 0x490], 0x00
    mov word [rax + 0x492], 0x008B      ; Type=0xB (32-bit busy TSS), Present, Ring 0
    mov dword [rax + 0x494], 0xFFFF
    mov qword [rax + 0x498], 0

    ; --- D. GDTR (Ofset 0x460) ve IDTR (Ofset 0x480) ---
    ; struct vmcb_seg = u16 sel/u16 attr/u32 limit/u64 base
    ; GDTR ve IDTR host işlemci tarafından VMRUN sırasında yüklenir
    mov rbx, [guest_gdt_phys]
    mov word [rax + 0x460], 0           ; gdtr selector
    mov word [rax + 0x462], 0           ; gdtr attrib
    mov dword [rax + 0x464], 0x17       ; gdtr limit (24 bytes = 3 entries)
    mov [rax + 0x468], rbx              ; gdtr base

    mov rbx, [guest_idt_phys]
    mov word [rax + 0x480], 0           ; idtr selector
    mov word [rax + 0x482], 0          ; idtr attrib
    mov dword [rax + 0x484], 0x7FF     ; idtr limit (256 vectors * 8)
    mov [rax + 0x488], rbx             ; idtr base

; =====================================================================
; ASIL ATESLEME VE SONSUZ SANALLAŞTIRMA DÖNGÜSÜ
; =====================================================================
.amd_loop:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push rax                ; Sahte dolgu (Padding)
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 'vmrun rax' gerçek donanım bytecode enjeksiyonu
    db 0x0F, 0x01, 0xD8

    ; --- BURASI VM-EXIT ANIDIR ---
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rax
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax                 ; VMCB adresini geri çek

    ; C++ Handler çağrısı (RDI = 2, RSI = VMCB Adresi)
    push rax
    mov rdi, 2
    mov rsi, rax
    call c_vmexit_handler
    pop rax

    jmp .amd_loop          ; Sonsuz sanallaştırma döngüsü


; =====================================================================
; INTEL VMLAUNCH (Garantili Yığın Hizalamalı)
; =====================================================================
asm_launch_intel_vm:
    push rbp
    mov rbp, rsp

    vmlaunch                ; Sanal makineyi ateşle!

    ; --- EĞER BURAYA DÜŞERSE: VMLAUNCH BAŞARISIZ OLMUŞTUR ---
    mov rax, 0              ; Başarısızlık sinyali
    mov rsp, rbp
    pop rbp
    ret

; Intel Host-RIP alanının uyanacağı tam nokta (VM-Exit Anı)
global vmm_exit_handler_stub
vmm_exit_handler_stub:
    ; 1. GPR'ları Stack'e Kaydet (Tam 16 adet push ile 16-byte sınırını koruyoruz)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push rax                ; SAHTE DOLGU (PADDING): 16-byte sınırını eşitlemek için RAX'i iki kez itiyoruz
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. C++ Handler'ı çağır. Parametreler: RDI = 1 (Intel), RSI = 0
    mov rdi, 1
    mov rsi, 0
    call c_vmexit_handler   ; Yığın tam 16-byte katı olduğu için güvenle çağrılır

    ; 3. GPR'ları Eksiksiz ve Kaydırmadan Geri Yükle
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rax                 ; Sahte dolguyu geri çek (rax geçici ezilebilir)
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 4. Sanal makineye geri dön
    vmresume
    hlt
    jmp vmm_exit_handler_stub
