#ifndef V_CPU_H
#define V_CPU_H

#include <cstdint>

// Her bir sanal işlemciyi (vCPU) temsil eden ortak yapı
struct VirtualCpu {
    uint64_t control_block_phys; // Intel için VMCS, AMD için VMCB fiziksel adresi
    uint32_t cpu_vendor_type;    // 1: Intel, 2: AMD
    bool is_active;
};

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Donanım türünü otomatik tespit ederek 4KB hizalı VMCS veya VMCB bloğu oluşturur.
     * @return VirtualCpu* Oluşturulan ortak vCPU nesnesi.
     */
    VirtualCpu* create_virtual_cpu();

#ifdef __cplusplus
}
#endif

#endif // V_CPU_H
