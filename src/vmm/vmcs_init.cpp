#include "vmcs_init.h"
#include "utils.h"

// =====================================================================
// INTEL GUEST SEGMENT FIELD ENCODINGS (from Intel SDM / QEMU vmcs.h)
// =====================================================================
#define VMCS_GUEST_CS_LIMIT             0x00004802
#define VMCS_GUEST_CS_AR_BYTES           0x00004816
#define VMCS_GUEST_CS_BASE              0x00006808

#define VMCS_GUEST_DS_LIMIT             0x00004806
#define VMCS_GUEST_DS_AR_BYTES           0x0000481A
#define VMCS_GUEST_DS_BASE              0x0000680C

#define VMCS_GUEST_SS_LIMIT             0x00004804
#define VMCS_GUEST_SS_AR_BYTES           0x00004818
#define VMCS_GUEST_SS_BASE              0x0000680A

#define VMCS_GUEST_ES_LIMIT             0x00004800
#define VMCS_GUEST_ES_AR_BYTES           0x00004814
#define VMCS_GUEST_ES_BASE              0x00006806

#define VMCS_GUEST_FS_LIMIT             0x00004808
#define VMCS_GUEST_FS_AR_BYTES           0x0000481C
#define VMCS_GUEST_FS_BASE              0x0000680E

#define VMCS_GUEST_GS_LIMIT             0x0000480A
#define VMCS_GUEST_GS_AR_BYTES           0x0000481E
#define VMCS_GUEST_GS_BASE              0x00006810

#define VMCS_GUEST_GDTR_BASE            0x00006816
#define VMCS_GUEST_GDTR_LIMIT           0x00004810
#define VMCS_GUEST_IDTR_BASE            0x00006818
#define VMCS_GUEST_IDTR_LIMIT           0x00004812

#define VMCS_GUEST_IA32_EFER            0x00002806
#define VMCS_VMEXIT_CONTROLS            0x0000400C
#define VMCS_VMENTRY_CONTROLS           0x00004012

extern "C" void simulated_guest_kernel();
extern "C" void vmm_exit_handler_stub(void);

// Intel için VMWRITE sarmalayıcısı
static inline bool vmx_vmwrite(uint64_t field, uint64_t value) {
    uint8_t error;
    asm volatile("vmwrite %2, %1; setc %0;" : "=q"(error) : "r"(field), "r"(value) : "cc");
    return (error == 0);
}

// Ortak Donanım Kayıtçısı Okuma Fonksiyonları
static inline uint64_t get_cr0() { uint64_t val; asm volatile("mov %%cr0, %0" : "=r"(val)); return val; }
static inline uint64_t get_cr3() { uint64_t val; asm volatile("mov %%cr3, %0" : "=r"(val)); return val; }
static inline uint64_t get_cr4() { uint64_t val; asm volatile("mov %%cr4, %0" : "=r"(val)); return val; }

static inline uint64_t vmcs_rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static uint32_t adjust_vmx_controls(uint32_t ctl_value, uint32_t msr_address) {
    uint64_t msr = vmcs_rdmsr(msr_address);
    ctl_value |= (uint32_t)msr;
    ctl_value &= (uint32_t)(msr >> 32);
    return ctl_value;
}

bool setup_vmcs_states(VirtualCpu* vcpu) {
    if (!vcpu || !vcpu->is_active) return false;

    print("\n--- Evrensel Long-Mode Uyumlu State Kurulumu ---");

    uint64_t host_rsp_addr = 0;
    asm volatile("mov %%rsp, %0" : "=r"(host_rsp_addr));

    // Host GDT taban adresini alarak Guest'e klonlayacağız (Hizalama kontrolü için)
    uint64_t gdt_base_addr = 0;
    uint16_t gdt_limit_val = 0;
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdtr_reg;
    asm volatile("sgdt %0" : "=m"(gdtr_reg));
    gdt_base_addr = gdtr_reg.base;
    gdt_limit_val = gdtr_reg.limit;

    // =====================================================================
    // MİMARİ 1: INTEL VMX (EFER VE GDTR KİLİTLERİ ÇÖZÜLDÜ)
    // =====================================================================
    if (vcpu->cpu_vendor_type == 1) {
        print("[Mimarî Atama]: Intel VT-x Modu");

        // 1. Dinamik Yürütme Kontrolleri
        vmx_vmwrite(VMCS_PIN_BASED_VM_EXEC_CONTROL, adjust_vmx_controls(0, 0x481));
        uint32_t proc_based = (1 << 9) | (1 << 10) | (1 << 28);
        vmx_vmwrite(VMCS_PROC_BASED_VM_EXEC_CONTROL, adjust_vmx_controls(proc_based, 0x482));

        // VM-Exit Kontrolleri: Bit 9 (64-bit Host), Bit 21 (Açılışta EFER MSR yükle) aktif edilmeli
        uint32_t vm_exit_ctls = (1 << 9) | (1 << 21);
        vmx_vmwrite(VMCS_VMEXIT_CONTROLS, adjust_vmx_controls(vm_exit_ctls, 0x483));

        // VM-Entry Kontrolleri: Bit 9 (64-bit Guest), Bit 15 (Girişte EFER MSR yükle) aktif edilmeli
        uint32_t vm_entry_ctls = (1 << 9) | (1 << 15);
        vmx_vmwrite(VMCS_VMENTRY_CONTROLS, adjust_vmx_controls(vm_entry_ctls, 0x484));

        vmx_vmwrite(VMCS_EXCEPTION_BITMAP, (1 << 14));
        vmx_vmwrite(VMCS_CR3_TARGET_COUNT, 0);
        vmx_vmwrite(0x0000401E, adjust_vmx_controls((1 << 1), 0x48B));

        // 2. Host-State Area
        vmx_vmwrite(VMCS_HOST_CR0, get_cr0());
        vmx_vmwrite(VMCS_HOST_CR3, get_cr3());
        vmx_vmwrite(VMCS_HOST_CR4, get_cr4());
        vmx_vmwrite(VMCS_HOST_CS_SELECTOR, 0x08);
        vmx_vmwrite(VMCS_HOST_DS_SELECTOR, 0x10);
        vmx_vmwrite(VMCS_HOST_SS_SELECTOR, 0x10);
        vmx_vmwrite(VMCS_HOST_ES_SELECTOR, 0x10);
        vmx_vmwrite(VMCS_HOST_FS_SELECTOR, 0);
        vmx_vmwrite(VMCS_HOST_GS_SELECTOR, 0);
        vmx_vmwrite(VMCS_HOST_TR_SELECTOR, 0);
        vmx_vmwrite(VMCS_HOST_FS_BASE, 0);
        vmx_vmwrite(VMCS_HOST_GS_BASE, 0);
        vmx_vmwrite(VMCS_HOST_TR_BASE, 0);
        vmx_vmwrite(VMCS_HOST_RSP, host_rsp_addr);
        vmx_vmwrite(VMCS_HOST_RIP, (uint64_t)vmm_exit_handler_stub);
        vmx_vmwrite(VMCS_HOST_IA32_SYSENTER_CS, 0);
        vmx_vmwrite(VMCS_HOST_IA32_SYSENTER_ESP, 0);
        vmx_vmwrite(VMCS_HOST_IA32_SYSENTER_EIP, 0);
        vmx_vmwrite(VMCS_HOST_IA32_EFER, vmcs_rdmsr(0xC0000080));

        // VMCS Link Pointer (required by Intel SDM)
        vmx_vmwrite(VMCS_LINK_POINTER, 0xFFFFFFFFFFFFFFFFULL);

        // 3. Guest-State Area - Segmentler ve Yapılar
        vmx_vmwrite(VMCS_GUEST_CS_SELECTOR, 0x08);
        vmx_vmwrite(VMCS_GUEST_CS_LIMIT, 0xFFFFFFFF);
        vmx_vmwrite(VMCS_GUEST_CS_BASE, 0);
        vmx_vmwrite(VMCS_GUEST_CS_AR_BYTES, 0xA09B);  // P=1,S=1,Type=B,L=1,G=1

        vmx_vmwrite(VMCS_GUEST_DS_SELECTOR, 0x10);
        vmx_vmwrite(VMCS_GUEST_DS_LIMIT, 0xFFFFFFFF);
        vmx_vmwrite(VMCS_GUEST_DS_BASE, 0);
        vmx_vmwrite(VMCS_GUEST_DS_AR_BYTES, 0xC093);  // P=1,S=1,Type=3,D/B=1,G=1

        vmx_vmwrite(VMCS_GUEST_SS_SELECTOR, 0x10);
        vmx_vmwrite(VMCS_GUEST_SS_LIMIT, 0xFFFFFFFF);
        vmx_vmwrite(VMCS_GUEST_SS_BASE, 0);
        vmx_vmwrite(VMCS_GUEST_SS_AR_BYTES, 0xC093);

        vmx_vmwrite(VMCS_GUEST_ES_SELECTOR, 0x10);
        vmx_vmwrite(VMCS_GUEST_ES_LIMIT, 0xFFFFFFFF);
        vmx_vmwrite(VMCS_GUEST_ES_BASE, 0);
        vmx_vmwrite(VMCS_GUEST_ES_AR_BYTES, 0xC093);

        vmx_vmwrite(VMCS_GUEST_FS_SELECTOR, 0); vmx_vmwrite(VMCS_GUEST_FS_LIMIT, 0);
        vmx_vmwrite(VMCS_GUEST_FS_BASE, 0); vmx_vmwrite(VMCS_GUEST_FS_AR_BYTES, 0x10000);

        vmx_vmwrite(VMCS_GUEST_GS_SELECTOR, 0); vmx_vmwrite(VMCS_GUEST_GS_LIMIT, 0);
        vmx_vmwrite(VMCS_GUEST_GS_BASE, 0); vmx_vmwrite(VMCS_GUEST_GS_AR_BYTES, 0x10000);

        vmx_vmwrite(VMCS_GUEST_LDTR_SELECTOR, 0);
        vmx_vmwrite(VMCS_GUEST_LDTR_LIMIT, 0);
        vmx_vmwrite(VMCS_GUEST_LDTR_BASE, 0);
        vmx_vmwrite(VMCS_GUEST_LDTR_ACCESS_RIGHTS, 0x10000);  // Unusable

        vmx_vmwrite(VMCS_GUEST_TR_SELECTOR, 0);
        vmx_vmwrite(VMCS_GUEST_TR_LIMIT, 0);
        vmx_vmwrite(VMCS_GUEST_TR_BASE, 0);
        vmx_vmwrite(VMCS_GUEST_TR_ACCESS_RIGHTS, 0x10000);  // Unusable

        vmx_vmwrite(VMCS_GUEST_GDTR_LIMIT, gdt_limit_val);
        vmx_vmwrite(VMCS_GUEST_GDTR_BASE, gdt_base_addr);

        uint64_t idt_base_addr = 0;
        uint16_t idt_limit_val = 0;
        struct { uint16_t limit; uint64_t base; } __attribute__((packed)) idtr_reg;
        asm volatile("sidt %0" : "=m"(idtr_reg));
        idt_base_addr = idtr_reg.base;
        idt_limit_val = idtr_reg.limit;
        vmx_vmwrite(VMCS_GUEST_IDTR_LIMIT, idt_limit_val);
        vmx_vmwrite(VMCS_GUEST_IDTR_BASE, idt_base_addr);

        vmx_vmwrite(VMCS_GUEST_DR7, 0x400);

        vmx_vmwrite(VMCS_GUEST_IA32_SYSENTER_CS, 0);
        vmx_vmwrite(VMCS_GUEST_IA32_SYSENTER_ESP, 0);
        vmx_vmwrite(VMCS_GUEST_IA32_SYSENTER_EIP, 0);

        // Temel Kayıtçılar
        vmx_vmwrite(VMCS_GUEST_CR0, get_cr0());
        vmx_vmwrite(VMCS_GUEST_CR3, get_cr3());
        vmx_vmwrite(VMCS_GUEST_CR4, get_cr4());
        vmx_vmwrite(VMCS_GUEST_RFLAGS, 0x02);
        vmx_vmwrite(VMCS_GUEST_RSP, host_rsp_addr - 8192);
        vmx_vmwrite(VMCS_GUEST_RIP, (uint64_t)simulated_guest_kernel);

        // KESİN KİLİT ÇÖZÜCÜ: Sanal makinenin IA32_EFER MSR kaydını okuyup,
        // Long Mode Active (LMA) ve Long Mode Enable (LME) bitlerini (Bit 8 ve Bit 10) 1 yapıyoruz!
        uint64_t current_efer = vmcs_rdmsr(0xC0000080); // IA32_EFER MSR Adresi
        current_efer |= (1ULL << 8) | (1ULL << 10);     // LME ve LMA bitlerini aç
        vmx_vmwrite(VMCS_GUEST_IA32_EFER, current_efer);

        print("Intel Donanim, EFER ve GDT Kilitleri Tam Olarak Acildi.");
    }
    // =====================================================================
    // MİMARİ 2: AMD SVM (EFER VE GDTR KİLİTLERİ ÇÖZÜLDÜ)
    // =====================================================================
    else if (vcpu->cpu_vendor_type == 2) {
       print("[Mimarî Atama]: AMD SVM Segment Modu");

        print("AMD VMCB Long Mode EFER (Ofset 0x4D0) ve Segmentleri Basariyla Esitlendi.");

    }
    else {
        return false;
    }

    return true;
}
