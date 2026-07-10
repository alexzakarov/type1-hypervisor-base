#include "vmm_loop.h"
#include "pmm.h"
#include "utils.h"

extern "C" {
    uint64_t asm_launch_intel_vm();
    void asm_launch_amd_vm(uint64_t vmcb_phys);
}

static inline uint64_t vmx_vmread(uint64_t field) {
    uint64_t value;
    asm volatile("vmread %1, %0" : "=r"(value) : "r"(field) : "cc");
    return value;
}

static inline void vmx_vmwrite(uint64_t field, uint64_t value) {
    asm volatile("vmwrite %1, %0" : : "r"(field), "r"(value) : "cc");
}

static uint64_t vmexit_count = 0;

extern "C" {
    uint64_t guest_gdt_phys = 0;
    uint64_t guest_idt_phys = 0;
}

static void setup_guest_idt_amd(uint64_t vmcb_phys) {
    guest_gdt_phys = pmm_alloc_page();
    guest_idt_phys = pmm_alloc_page();
    if (guest_gdt_phys == 0 || guest_idt_phys == 0) return;

    uint64_t* gdt = (uint64_t*)guest_gdt_phys;
    gdt[0] = 0;
    gdt[1] = 0x00CF9B000000FFFFULL;
    gdt[2] = 0x00CF93000000FFFFULL;

    uint8_t* idt = (uint8_t*)guest_idt_phys;

    uint64_t handler_addr = guest_idt_phys + 0xFF0;
    idt[0xFF0] = 0xCF;

    uint16_t handler_low = (uint16_t)(handler_addr & 0xFFFF);
    uint16_t handler_high = (uint16_t)((handler_addr >> 16) & 0xFFFF);

    uint32_t vec_0x20_off = 0x20 * 8;
    idt[vec_0x20_off + 0] = (uint8_t)(handler_low & 0xFF);
    idt[vec_0x20_off + 1] = (uint8_t)(handler_low >> 8);
    idt[vec_0x20_off + 2] = 0x08;
    idt[vec_0x20_off + 3] = 0x00;
    idt[vec_0x20_off + 4] = 0x00;
    idt[vec_0x20_off + 5] = 0x8E;
    idt[vec_0x20_off + 6] = (uint8_t)(handler_high & 0xFF);
    idt[vec_0x20_off + 7] = (uint8_t)(handler_high >> 8);

    print("Guest GDT+IDT hazir.");
}

void start_virtual_machine_loop(VirtualCpu* vcpu) {
    if (!vcpu || !vcpu->is_active) return;

    uint32_t vendor_type = vcpu->cpu_vendor_type;
    uint64_t control_block = vcpu->control_block_phys;

    print("\n--- Asama 6: Sanal Makine Dongusu Basliyor ---");

    if (vendor_type == 1) {
        print("Intel VMLAUNCH dongusu baslatiliyor...");
        uint64_t status = asm_launch_intel_vm();
        if (status == 0) {
            print("Hata: VMLAUNCH donanimsal olarak reddedildi!");
        }
    }
    else if (vendor_type == 2) {
        setup_guest_idt_amd(control_block);
        print("AMD VMRUN dongusu baslatiliyor...");
        asm_launch_amd_vm(control_block);
    }
}

extern "C" void c_vmexit_handler(uint64_t vcpu_vendor_type, uint64_t vmcb_ptr)
{
    vmexit_count++;
    uint64_t exit_reason = 0;

    if (vcpu_vendor_type == 1) {
        exit_reason = vmx_vmread(VMCS_EXIT_REASON);
    }
    else if (vcpu_vendor_type == 2) {
        uint64_t* exit_code_ptr = (uint64_t*)(vmcb_ptr + VMCB_EXIT_CODE);
        exit_reason = *exit_code_ptr;
    }

    if (vmexit_count == 1) {
        if (vcpu_vendor_type == 1)
            print("\n[VM-EXIT INTEL] Ilk cikis! Sebep (Hex): ");
        else
            print("\n[VM-EXIT AMD] Ilk cikis! Sebep (Hex): ");
        print_hex(exit_reason);
    }

    if (vcpu_vendor_type == 1) {
        vmx_vmwrite(VMCS_VM_ENTRY_INTR_INFO, 0);

        switch (exit_reason) {
        case VMX_EXIT_REASON_HLT: {
            uint64_t guest_rip = vmx_vmread(VMCS_GUEST_RIP);
            vmx_vmwrite(VMCS_GUEST_RIP, guest_rip + 1);

            if (vmexit_count % 100000 == 0 && vmexit_count > 0) {
                vmx_vmwrite(VMCS_VM_ENTRY_INTR_INFO, (1ULL << 31) | 0x20);
                print("\n[IRQ 0x20 Intel] Event injection @ exit ");
                print_int(vmexit_count);
            }
            if (vmexit_count <= 10) {
                print("\n[VM-EXIT HLT] Sayac: ");
                print_int(vmexit_count);
            }
            break;
        }
        case VMX_EXIT_REASON_CPUID: {
            uint64_t guest_rip = vmx_vmread(VMCS_GUEST_RIP);
            vmx_vmwrite(VMCS_GUEST_RIP, guest_rip + 2);
            break;
        }
        default: {
            print("\n[VM-EXIT INTEL] Bilinmeyen cikis kodu: ");
            print_hex(exit_reason);
            print("\nSistem durduruluyor.");
            while (1) { asm volatile("hlt"); }
            break;
        }
        }
    }
    else if (vcpu_vendor_type == 2) {
        uint32_t* event_inj = (uint32_t*)(vmcb_ptr + VMCB_EVENT_INJ);
        uint64_t* rip_ptr = (uint64_t*)(vmcb_ptr + VMCB_GUEST_RIP);

        switch (exit_reason) {
        case SVM_EXIT_HLT: {
            *rip_ptr += 1;

            if (vmexit_count % 100000 == 0 && vmexit_count > 0) {
                *event_inj = SVM_EVTINJ_VALID | SVM_EVTINJ_TYPE_INTR | 0x20;
                print("\n[IRQ 0x20] Event injection @ exit ");
                print_int(vmexit_count);
            }
            if (vmexit_count <= 10) {
                print("\n[VM-EXIT HLT] Sayac: ");
                print_int(vmexit_count);
            }
            break;
        }
        case SVM_EXIT_CPUID: {
            *rip_ptr += 2;
            break;
        }
        default: {
            print("\n[VM-EXIT AMD] Bilinmeyen cikis kodu: ");
            print_hex(exit_reason);
            print("\nSistem durduruluyor.");
            while (1) { asm volatile("hlt"); }
            break;
        }
        }
    }
}