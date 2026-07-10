#pragma once
#include <cstdint>


// x86_64 Sayfalama Bayrakları (Page Table Entry Flags)
#define PAGE_PRESENT  (1ULL << 0)   // Sayfa bellekte mevcut
#define PAGE_WRITABLE (1ULL << 1)   // Sayfa yazılabilir
#define PAGE_USER     (1ULL << 2)   // Kullanıcı modu erişebilir (Hypervisor için 0 kalmalı)
#define PAGE_LARGE    (1ULL << 7)   // 2MB büyüklüğünde sayfa (PD seviyesinde kullanılır)

extern "C" {

    /**
     * @brief Host (Hypervisor) için sıfırdan x86_64 4-Seviyeli sayfa tablolarını başlatır.
     *
     * Bu fonksiyon PMM'i kullanarak dinamik bellek ayırır ve ilk etapta
     * tüm fiziksel belleği (Identity Mapping ile) 2MB'lık sayfalar halinde haritalar.
     */
    void init_vmm();

    /**
     * @brief Oluşturulan yeni PML4 sayfa tablosunu işlemciye (CR3 register'ına) yükler.
     */
    void vmm_switch_pml4(uint64_t pml4_address);

}
