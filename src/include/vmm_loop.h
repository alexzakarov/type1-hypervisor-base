#ifndef VMM_LOOP_H
#define VMM_LOOP_H

#include <cstdint>
#include "v_cpu.h"

// Intel VMX Exit Reason Kodları
#define VMX_EXIT_REASON_EXCEPTION       0
#define VMX_EXIT_REASON_TRIPLE_FAULT    2
#define VMX_EXIT_REASON_CPUID           10
#define VMX_EXIT_REASON_HLT             12
#define VMX_EXIT_REASON_INVD            13
#define VMX_EXIT_REASON_VMCALL          18
#define VMX_EXIT_REASON_CR_ACCESS       28
#define VMX_EXIT_REASON_IO_INSTRUCTION  30

// Intel VMCS Field Encodings (emülasyon için)
#define VMCS_EXIT_REASON                0x00004402
#define VMCS_GUEST_RIP                  0x0000681E
#define VMCS_GUEST_RSP                  0x0000681C
#define VMCS_GUEST_RFLAGS               0x00006820
#define VMCS_VM_ENTRY_INTR_INFO         0x00004016
#define VMCS_VM_ENTRY_EXCEPTION_ERR_CODE 0x00004018

// AMD SVM Exit Code Kodları
#define SVM_EXIT_CR0_READ               0x00000000
#define SVM_EXIT_CR3_WRITE              0x00000013
#define SVM_EXIT_EXCP_PF                0x0000004E // #PF Page Fault
#define SVM_EXIT_CPUID                  0x00000072
#define SVM_EXIT_HLT                    0x00000078
#define SVM_EXIT_VMMCALL                0x00000081
#define SVM_EXIT_ERR                    0xFFFFFFFF

// AMD SVM VMCB Control Area offsets (QEMU svm.h layout)
#define VMCB_EXIT_CODE                  0x070
#define VMCB_EXIT_INFO_1                0x078
#define VMCB_EXIT_INFO_2                0x080
#define VMCB_EXIT_INT_INFO              0x088
#define VMCB_NESTED_CTL                 0x090
#define VMCB_EVENT_INJ                  0x0A8
#define VMCB_EVENT_INJ_ERR              0x0AC
#define VMCB_NESTED_CR3                 0x0B0

// AMD SVM VMCB Save Area offsets (VMCB base + offset)
#define VMCB_GUEST_EFER                 0x4D0
#define VMCB_GUEST_CR0                  0x558
#define VMCB_GUEST_CR3                  0x550
#define VMCB_GUEST_CR4                  0x548
#define VMCB_GUEST_RFLAGS               0x570
#define VMCB_GUEST_RIP                  0x578
#define VMCB_GUEST_IDTR                 0x480
#define VMCB_GUEST_RSP                  0x5D8
#define VMCB_GUEST_RAX                  0x5F8

// AMD SVM Event Injection encoding
#define SVM_EVTINJ_VALID                (1U << 31)
#define SVM_EVTINJ_VALID_ERR            (1U << 11)
#define SVM_EVTINJ_TYPE_SHIFT           8
#define SVM_EVTINJ_TYPE_INTR            (0U << SVM_EVTINJ_TYPE_SHIFT)
#define SVM_EVTINJ_TYPE_NMI             (2U << SVM_EVTINJ_TYPE_SHIFT)
#define SVM_EVTINJ_TYPE_EXEPT           (3U << SVM_EVTINJ_TYPE_SHIFT)
#define SVM_EVTINJ_TYPE_SOFT            (4U << SVM_EVTINJ_TYPE_SHIFT)

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Sanal makine döngüsünü başlatır. İşlemci türüne göre ilgili Assembly fırlatıcısını çağırır.
     * @param vcpu Yapılandırılmış ve hazır olan vCPU nesnesi.
     */
    void start_virtual_machine_loop(VirtualCpu* vcpu);

    /**
     * @brief Assembly stub'ından çağrılan ortak VM-Exit analiz ve emülasyon fonksiyonu.
     * @param vcpu_vendor_type 1: Intel, 2: AMD
     * @param vmcb_ptr AMD için VMCB bellek adresi (Intel için 0 paslanır)
     */
    void c_vmexit_handler(uint64_t vcpu_vendor_type, uint64_t vmcb_ptr);

#ifdef __cplusplus
}
#endif

#endif // VMM_LOOP_H
