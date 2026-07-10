#ifndef EPT_H
#define EPT_H

#include <cstdint>
#include "v_cpu.h"

// =====================================================================
// INTEL EPT (EXTENDED PAGE TABLES) BAYRAKLARI
// =====================================================================
#define EPT_READ            (1ULL << 0)   // Sayfa okunabilir
#define EPT_WRITE           (1ULL << 1)   // Sayfa yazılabilir
#define EPT_EXECUTE         (1ULL << 2)   // Sayfa içinde kod çalıştırılabilir
#define EPT_MEMORY_TYPE_WB  (6ULL << 3)   // Önbellek Tipi: Write-Back (Zorunlu)
#define EPT_LARGE_PAGE      (1ULL << 7)   // 2MB Geniş sayfa belirteci

// =====================================================================
// AMD NPT (NESTED PAGE TABLES) BAYRAKLARI (Standart MMU Yapısı)
// =====================================================================
#define NPT_PRESENT         (1ULL << 0)   // Sayfa mevcut
#define NPT_WRITABLE        (1ULL << 1)   // Sayfa yazılabilir
#define NPT_USER            (1ULL << 2)   // Kullanıcı modu erişimi
#define NPT_LARGE_PAGE      (1ULL << 7)   // 2MB Geniş sayfa belirteci

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Donanım türüne göre 4 Seviyeli EPT (Intel) veya NPT (AMD) tablosu oluşturur.
     * Sanal makinenin ilk 4 GB'lık alanını GPA = HPA (Identity Mapping) olarak haritalar.
     *
     * @param vcpu Tabloların bağlanacağı vCPU nesnesi
     * @return true Başarıyla oluşturulup vCPU bloğuna işlendiyse, false Hata durumunda.
     */
    bool setup_memory_virtualization(VirtualCpu* vcpu);

#ifdef __cplusplus
}
#endif

#endif // EPT_H
