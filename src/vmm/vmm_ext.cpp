#include "vmm_ext.h"
#include "pmm.h"
#include "utils.h"


extern "C" uint64_t asm_execute_vmxon(uint64_t phys_address);

// Satır içi (Inline) Assembly ile CPUID Yardımcı Fonksiyonu
static inline void cpuid(uint32_t code, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    asm volatile("cpuid"
                 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                 : "a"(code), "c"(0));
}

// Satır içi Assembly ile MSR Okuma Fonksiyonu
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

// Satır içi Assembly ile MSR Yazma Fonksiyonu
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

// 1. ADIM: İşlemci Üreticisini (Vendor) Tespit Etme
static CpuVendor get_cpu_vendor() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, &eax, &ebx, &ecx, &edx);

    // Intel işlemciler ebx, edx, ecx içinde "GenuntelineI" yazar
    if (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e) {
        print("Islemci Ureticisi: Intel (GenuineIntel)");
        return VENDOR_INTEL;
    }
    // AMD işlemciler ebx, edx, ecx içinde "AuthenticAMD" yazar
    else if (ebx == 0x68747541 && edx == 0x69746e65 && ecx == 0x444d4163) {
        print("Islemci Ureticisi: AMD (AuthenticAMD)");
        return VENDOR_AMD;
    }

    print("Islemci Ureticisi Belirlenemedi!");
    return VENDOR_UNKNOWN;
}

// Mevcut cpuid, rdmsr, wrmsr yardımcı fonksiyonlarınız burada kalıyor...

static bool check_and_enable_vtx() {
    uint32_t eax, ebx, ecx, edx;

    // CPUID Kontrolü
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if (!(ecx & (1 << 5))) {
        print("Hata: Bu Intel islemci VT-x desteklemiyor!");
        return false;
    }

    // BIOS Kilit Kontrolü (MSR 0x3A)
    uint64_t feature_control = rdmsr(0x3A);
    if (!(feature_control & (1ULL << 0))) {
        feature_control |= (1ULL << 0) | (1ULL << 2);
        wrmsr(0x3A, feature_control);
    } else if (!(feature_control & (1ULL << 2))) {
        print("Hata: VT-x BIOS uzerinden kapatilmis!");
        return false;
    }

    // 1. CR4.VMXE (Bit 13) Aktif Ediliyor
    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 13);
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
    print("CR4.VMXE biti başarıyla set edildi.");

    // 2. VMXON Region (Bellek Bölgesi) Hazırlığı
    // PMM modülümüzü kullanarak tam 4KB hizalı temiz bir fiziksel sayfa ayırıyoruz
    uint64_t vmxon_region_phys = pmm_alloc_page();
    if (vmxon_region_phys == 0) {
        print("Hata: VMXON bellek bölgesi tahsis edilemedi!");
        return false;
    }

    // IA32_VMX_BASIC MSR (0x480) adresinden işlemcinin sanallaştırma revizyon numarasını alıyoruz
    uint64_t vmx_basic = rdmsr(0x480);
    uint32_t revision_id = (uint32_t)vmx_basic; // İlk 32 bit revizyon numarasıdır

    // Ayrılan fiziksel bellek alanının ilk 4 byte'ına bu numarayı yazmak zorundayız
    uint32_t* vmxon_ptr = (uint32_t*)vmxon_region_phys;
    vmxon_ptr[0] = revision_id;

    print("VMXON Revizyon ID Alindi: ");
    print_hex(revision_id);

    // 3. Assembly Üzerinden VMXON Komutunun Çalıştırılması
    uint64_t success = asm_execute_vmxon(vmxon_region_phys);
    if (success == 0) {
        print("Hata: VMXON komutu işlemci seviyesinde BAŞARISIZ oldu!");
        return false;
    }

    print("Intel VT-x (VMXON) Donanım Motoru Tamamen ÇALIŞTIRILDI.");
    return true;
}

// 3. ADIM: AMD (SVM) Etkinleştirme Mantığı
static bool check_and_enable_svm() {
    uint32_t eax, ebx, ecx, edx;

    // CPUID.0x80000001:ECX.SVM[bit 2] -> 1 ise AMD güvenli sanal makine destekliyor demektir.
    cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    if (!(ecx & (1 << 2))) {
        print("Hata: Bu AMD islemci SVM (AMD-V) teknolojisini DESTEKLEMIYOR!");
        return false;
    }
    print("AMD SVM Donanim Destegi Mevcut.");

    // VM_CR MSR (Address: 0xC0010114) Kontrolü
    // AMD mimarisinde SVM kilidi bu adrestedir. Bit 4 (SVMDIS) kilidi temsil eder.
    uint64_t vm_cr = rdmsr(0xC0010114);
    if (vm_cr & (1ULL << 4)) {
        print("Hata: AMD SVM kilidi BIOS uzerinden KAPATILMIS!");
        return false;
    }

    // AMD'de EFER (Extended Feature Enable Register) MSR (Address: 0xC0000080)
    // içindeki SVME (Bit 12) biti aktif edilmelidir.
    uint64_t efer = rdmsr(0xC0000080);
    efer |= (1ULL << 12); // SVME Bitini 1 yap
    wrmsr(0xC0000080, efer);

    print("AMD SVM (AMD-V) Modu Basariyla AKTIF EDILDI.");
    return true;
}

// Ana Kontrol Döngüsü
bool init_virtualization_extensions() {
    print("\n--- Asama 2: Sanallastirma Uzantilarinin Kontrolu ---");

    CpuVendor vendor = get_cpu_vendor();

    if (vendor == VENDOR_INTEL) {
        return check_and_enable_vtx();
    }
    else if (vendor == VENDOR_AMD) {
        return check_and_enable_svm();
    }

    return false;
}
