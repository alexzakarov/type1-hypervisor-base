#ifndef VMCS_INIT_H
#define VMCS_INIT_H

#include <cstdint>
#include "v_cpu.h"

// =====================================================================
// INTEL VMCS COMPONENT ENCODINGS (CONTROL FIELDS)
// =====================================================================
#define VMCS_PIN_BASED_VM_EXEC_CONTROL          0x00004000
#define VMCS_PROC_BASED_VM_EXEC_CONTROL         0x00004002
#define VMCS_SECONDARY_VM_EXEC_CONTROL          0x0000401E
#define VMCS_EXCEPTION_BITMAP                   0x00004004
#define VMCS_PAGE_FAULT_ERROR_CODE_MASK         0x00004006
#define VMCS_PAGE_FAULT_ERROR_CODE_MATCH        0x00004008
#define VMCS_CR3_TARGET_COUNT                   0x0000400A

// Önceki aşamadan kalan State Area tanımları...
#define VMCS_GUEST_CR0                          0x00006800
#define VMCS_GUEST_CR3                          0x00006802
#define VMCS_GUEST_CR4                          0x00006804
#define VMCS_GUEST_RIP                          0x0000681E
#define VMCS_GUEST_RSP                          0x0000681C
#define VMCS_GUEST_RFLAGS                       0x00006820
#define VMCS_GUEST_CS_SELECTOR                  0x00000802
#define VMCS_GUEST_DS_SELECTOR                  0x00000806
#define VMCS_GUEST_SS_SELECTOR                  0x00000804
#define VMCS_GUEST_ES_SELECTOR                  0x00000800
#define VMCS_GUEST_FS_SELECTOR                  0x00000808
#define VMCS_GUEST_GS_SELECTOR                  0x0000080A
#define VMCS_GUEST_LDTR_SELECTOR                0x0000080C
#define VMCS_GUEST_TR_SELECTOR                  0x0000080E

#define VMCS_HOST_CR0                           0x00006C00
#define VMCS_HOST_CR3                           0x00006C02
#define VMCS_HOST_CR4                           0x00006C04
#define VMCS_HOST_RIP                           0x00006C16
#define VMCS_HOST_RSP                           0x00006C14
#define VMCS_HOST_CS_SELECTOR                   0x00000C02
#define VMCS_HOST_DS_SELECTOR                   0x00000C06
#define VMCS_HOST_SS_SELECTOR                   0x00000C04
#define VMCS_HOST_ES_SELECTOR                   0x00000C00
#define VMCS_HOST_FS_SELECTOR                   0x00000C08
#define VMCS_HOST_GS_SELECTOR                   0x00000C0A
#define VMCS_HOST_TR_SELECTOR                   0x00000C0C
#define VMCS_HOST_FS_BASE                       0x00006C06
#define VMCS_HOST_GS_BASE                       0x00006C08
#define VMCS_HOST_TR_BASE                       0x00006C0A
#define VMCS_HOST_IA32_SYSENTER_CS              0x00004C00
#define VMCS_HOST_IA32_SYSENTER_ESP             0x00006C10
#define VMCS_HOST_IA32_SYSENTER_EIP             0x00006C12
#define VMCS_HOST_IA32_EFER                     0x00002C02

#define VMCS_LINK_POINTER                       0x00002800
#define VMCS_GUEST_DR7                          0x0000681A
#define VMCS_GUEST_IA32_SYSENTER_CS             0x0000482A
#define VMCS_GUEST_IA32_SYSENTER_ESP            0x00006824
#define VMCS_GUEST_IA32_SYSENTER_EIP            0x00006826
#define VMCS_GUEST_LDTR_LIMIT                   0x0000480C
#define VMCS_GUEST_TR_LIMIT                     0x0000480E
#define VMCS_GUEST_LDTR_ACCESS_RIGHTS           0x00004820
#define VMCS_GUEST_TR_ACCESS_RIGHTS             0x00004822
#define VMCS_GUEST_LDTR_BASE                    0x00006812
#define VMCS_GUEST_TR_BASE                      0x00006814

// =====================================================================
// AMD SVM VMCB STRUCT MAP (Geliştirilmiş Kontrol Alanları)
// =====================================================================
struct __attribute__((packed)) AmdVmcbLayout {
    // 0x000 - 0x3FF: Kontrol Alanları (Control Fields & Intercepts)
    uint32_t intercept_cr;              // CR0-CR15 okuma/yazma kesmeleri
    uint32_t intercept_dr;              // DR0-DR15 okuma/yazma kesmeleri
    uint32_t intercept_exceptions;      // İşlemci istisnaları (0-31 arası hatalar)
    uint32_t intercept_vector1;         // Bit 0: INTR, Bit 1: NMI, Bit 2: SMI
    uint32_t intercept_vector2;         // Bit 0: VMRUN, Bit 1: VMMCALL, Bit 2: HLTS
    uint8_t  reserved1[40];
    uint64_t iopm_base_pa;              // I/O Port İzin Haritası (Fiziksel Adres)
    uint64_t msrpm_base_pa;             // MSR İzin Haritası (Fiziksel Adres)
    uint64_t tsc_offset;
    uint32_t asid;                      // Address Space ID
    uint32_t tlb_control;
    uint8_t  reserved2[944];            // 0x400 Ofsetine (State Alanına) kadar tam hizalama

    // 0x400 - 0xFFF: Durum Alanları (State Save Area)
    uint64_t guest_cr0;
    uint64_t guest_cr2;
    uint64_t guest_cr3;
    uint64_t guest_cr4;
    uint64_t guest_dr6;
    uint64_t guest_dr7;
    uint64_t guest_rflags;
    uint64_t guest_rip;
    uint8_t  reserved3[8];
    uint64_t guest_rsp;
    uint8_t  reserved4[24];
    uint64_t guest_rax;

    uint16_t guest_cs_selector;
    uint16_t guest_cs_attrib;
    uint32_t guest_cs_limit;
    uint64_t guest_cs_base;

    uint16_t guest_ds_selector;
    uint16_t guest_ds_attrib;
    uint32_t guest_ds_limit;
    uint64_t guest_ds_base;

    uint16_t guest_ss_selector;
    uint16_t guest_ss_attrib;
    uint32_t guest_ss_limit;
    uint64_t guest_ss_base;

    uint8_t  reserved5[2912];
};

#ifdef __cplusplus
extern "C" {
#endif

bool setup_vmcs_states(VirtualCpu* vcpu);

#ifdef __cplusplus
}
#endif

#endif // VMCS_INIT_H
