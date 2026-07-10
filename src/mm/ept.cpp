#include "ept.h"
#include "pmm.h"
#include "utils.h"

// vmcs_init.cpp ve vmm_loop_asm.asm dosyalarının görebilmesi için küresel adres
extern "C" {
    uint64_t slat_pml4_phys = 0;
}

static inline bool ept_intel_vmwrite(uint64_t field, uint64_t value) {
    uint8_t error;
    asm volatile("vmwrite %2, %1; setc %0;" : "=q"(error) : "r"(field), "r"(value) : "cc");
    return (error == 0);
}

// PMM'den sayfa talep edip içini temizleyen yardımcı işlev
static uint64_t allocate_and_clear_page() {
    uint64_t page_addr = pmm_alloc_page();
    if (page_addr == 0) return 0;

    // KESİN DÜZELTME 1: Dönen sayfa adresinin tam 4KB hizalı olmasını mîmâri olarak garantiye alıyoruz
    page_addr = page_addr & ~0xFFFULL;

    uint64_t* ptr = (uint64_t*)page_addr;
    for (int i = 0; i < 512; i++) {
        ptr[i] = 0;
    }
    return page_addr;
}

bool setup_memory_virtualization(VirtualCpu* vcpu) {
    if (!vcpu || !vcpu->is_active) return false;

    print("\n--- Asama 4: Ikinci Seviye Adres Donusumu (EPT/NPT) ---");

    // 1. En üst seviye PML4 tablosunu ayır ve maskele
    slat_pml4_phys = allocate_and_clear_page();
    if (slat_pml4_phys == 0) {
        print("Hata: PML4 bellek sayfasi ayrilamadi!");
        return false;
    }
    uint64_t* slat_pml4 = (uint64_t*)slat_pml4_phys;

    // 2. Bir alt katman olan PDPT tablosunu ayır ve maskele
    uint64_t slat_pdpt_phys = allocate_and_clear_page();
    if (slat_pdpt_phys == 0) {
        print("Hata: PDPT bellek sayfasi ayrilamadi!");
        return false;
    }
    uint64_t* slat_pdpt = (uint64_t*)slat_pdpt_phys;

    // =====================================================================
    // MİMARİ KOLU 1: INTEL VMX (EPT - EXTENDED PAGE TABLES)
    // =====================================================================
    if (vcpu->cpu_vendor_type == 1) {
        print("[Islemci Modu]: Intel EPT Yapisi Kuruluyor...");

        // PML4'ün 0. indeksine PDPT adresini bağla
        slat_pml4[0] = slat_pdpt_phys | EPT_READ | EPT_WRITE | EPT_EXECUTE;

        for (uint64_t gb = 0; gb < 4; gb++) {
            uint64_t intel_pd_phys = allocate_and_clear_page();
            if (intel_pd_phys == 0) return false;
            uint64_t* intel_pd = (uint64_t*)intel_pd_phys;

            slat_pdpt[gb] = intel_pd_phys | EPT_READ | EPT_WRITE | EPT_EXECUTE;

            for (uint64_t entry = 0; entry < 512; entry++) {
                uint64_t physical_address = (gb * 1024ULL * 1024ULL * 1024ULL) + (entry * 2ULL * 1024ULL * 1024ULL);
                intel_pd[entry] = physical_address | EPT_READ | EPT_WRITE | EPT_EXECUTE | EPT_MEMORY_TYPE_WB | EPT_LARGE_PAGE;
            }
        }

        uint64_t ept_pointer = slat_pml4_phys | 0x1E;
        if (!ept_intel_vmwrite(0x0000201A, ept_pointer)) return false;
        ept_intel_vmwrite(0x0000401E, (1ULL << 1));

        print("Intel EPT Tamamlandi. EPTP: ");
        print_hex(ept_pointer);
    }
    // =====================================================================
    // MİMARİ KOLU 2: AMD SVM (NPT - NESTED PAGE TABLES)
    // =====================================================================
    else if (vcpu->cpu_vendor_type == 2) {
        print("[Islemci Modu]: AMD NPT Yapisi Kuruluyor...");

        // KESİN DÜZELTME 2: Dizinin 0. indeksine atama yapılıyor ve adres el ile 4KB maskeleniyor
        slat_pml4[0] = (slat_pdpt_phys & ~0xFFFULL) | NPT_PRESENT | NPT_WRITABLE | NPT_USER;

        // İlk 4 GB'lık alanı 2MB'lık geniş sayfalarla haritalandır
        for (uint64_t gb = 0; gb < 4; gb++) {
            uint64_t amd_pd_phys = allocate_and_clear_page();
            if (amd_pd_phys == 0) {
                print("Hata: AMD NPT PD sayfasi ayrilamadi!");
                return false;
            }
            uint64_t* amd_pd = (uint64_t*)amd_pd_phys;

            // KESİN DÜZELTME 3: PDPT hiyerarşik bağlantısında alt adres el ile 4KB'a zorlanıyor
            slat_pdpt[gb] = (amd_pd_phys & ~0xFFFULL) | NPT_PRESENT | NPT_WRITABLE | NPT_USER;

            for (uint64_t entry = 0; entry < 512; entry++) {
                uint64_t physical_address = (gb * 1024ULL * 1024ULL * 1024ULL) + (entry * 2ULL * 1024ULL * 1024ULL);

                // KESİN DÜZELTME 4: Gerçek donanım haritalama adresi (HPA) milimetrik maskeleniyor
                amd_pd[entry] = (physical_address & ~0xFFFULL) | NPT_PRESENT | NPT_WRITABLE | NPT_USER | NPT_LARGE_PAGE;
            }
        }

        print("AMD NPT Alt Seviye Hizalamalari Basariyla Sabitlendi. nested_cr3: ");
        print_hex(slat_pml4_phys);
    }
    else {
        return false;
    }

    return true;
}
