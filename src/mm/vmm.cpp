#include "vmm.h"
#include "pmm.h"
#include "utils.h"

// Yardımcı Fonksiyon: PMM'den sayfa alıp içini temizler
static uint64_t allocate_clean_page() {
    uint64_t page_addr = pmm_alloc_page();
    if (page_addr == 0) return 0;
    // Temizleme işlemini garantilemek için uint64_t* tipine dönüştürüyoruz
    uint64_t* ptr = (uint64_t*)page_addr;
    for (int i = 0; i < 512; i++) {
        ptr[i] = 0;
    }
    return page_addr;
}

void init_vmm() {
    // 1. PML4 Tablosunu ayır
    uint64_t pml4_phys = allocate_clean_page();
    if (!pml4_phys) {
        print("Hata: VMM icin PML4 sayfasi ayrilamadi!");
        return;
    }
    uint64_t* pml4_table = (uint64_t*)pml4_phys;

    // 2. PDPT Tablosunu ayır
    uint64_t pdpt_phys = allocate_clean_page();
    if (!pdpt_phys) {
        print("Hata: VMM icin PDPT sayfasi ayrilamadi!");
        return;
    }
    uint64_t* pdpt_table = (uint64_t*)pdpt_phys;

    // KRİTİK DÜZELTME: PML4 tablosunun İLK ELEMANINA (indeks 0) PDPT adresini ve bayraklarını yazıyoruz
    pml4_table[0] = pdpt_phys | PAGE_PRESENT | PAGE_WRITABLE;

    // 3. İlk 4 GB'lık alanı Identity Mapping (Sanal=Fiziksel) ile haritalandırmak için 4 adet PD oluştur
    for (uint64_t gb = 0; gb < 4; gb++) {
        uint64_t pd_phys = allocate_clean_page();
        if (!pd_phys) {
            print("Hata: VMM icin PD sayfasi ayrilamadi!");
            return;
        }
        uint64_t* pd_table = (uint64_t*)pd_phys;

        // PDPT tablosunun ilgili indeksine PD adresini kaydet
        pdpt_table[gb] = pd_phys | PAGE_PRESENT | PAGE_WRITABLE;

        // Her bir PD tablosunun içini 512 adet 2MB'lık sayfayla doldur
        for (uint64_t entry = 0; entry < 512; entry++) {
            // Tam fiziksel adresi hesapla (gb * 1GB + entry * 2MB)
            uint64_t physical_address = (gb * 1024ULL * 1024ULL * 1024ULL) + (entry * 2ULL * 1024ULL * 1024ULL);

            // PAGE_LARGE bayrağı ile bu sayfanın 2MB olduğunu işlemciye bildiriyoruz
            pd_table[entry] = physical_address | PAGE_PRESENT | PAGE_WRITABLE | PAGE_LARGE;
        }
    }

    // 4. Hazırlanan PML4 tablosunun fiziksel adresini CR3'e yükle
    vmm_switch_pml4(pml4_phys);

    print("VMM Katmani (Host Sayfa Tablolari) Aktif Edildi.");
}

void vmm_switch_pml4(uint64_t pml4_address) {
    asm volatile("mov %0, %%cr3" : : "r"(pml4_address) : "memory");
}
