#include "v_cpu.h"
#include "pmm.h"
#include "utils.h"

// Satır içi (Inline) Assembly ile CPUID Yardımcı Fonksiyonu
static inline void v_cpuid(uint32_t code, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(code), "c"(0));
}

// Satır içi Assembly ile MSR Okuma Fonksiyonu
static inline uint64_t v_rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

// INTEL ÖZEL: VMCS'i işlemciye yükleyen garantili stack/hizalı assembly fonksiyonu
static inline bool asm_load_vmcs(uint64_t vmcs_phys_addr) {
    uint8_t error = 0;
    asm volatile(
        "push %1; "             // Adresi stack'e güvenle it (Hizalama garantisi)
        "vmptrld (%%rsp); "     // Yığındaki adresten VMCS'i yükle
        "pop %%rax; "           // Yığını temizle
        "setc %0; "             // Hata durumunu (Carry Flag) yakala
        : "=q"(error)
        : "r"(vmcs_phys_addr)
        : "rax", "cc", "memory"
    );
    return (error == 0);
}

// Global vCPU statik havuzu
static VirtualCpu global_vcpu;

VirtualCpu* create_virtual_cpu()
{
    print("\n--- Asama 3: Evrensel Sanal Makine Kontrol Yapisi (VMCS/VMCB) ---");

    // 1. İşlemci Türünü Çalışma Anında (CPUID ile) Yeniden Kontrol Et
    uint32_t eax, ebx, ecx, edx;
    v_cpuid(0, &eax, &ebx, &ecx, &edx);

    int vendor = 0; // 1: Intel, 2: AMD
    if (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e) {
        vendor = 1; // Intel
    } else if (ebx == 0x68747541 && edx == 0x69746e65 && ecx == 0x444d4163) {
        vendor = 2; // AMD
    }

    // 2. PMM'den her iki mimari için de şart olan 4KB'lık temiz kontrol sayfasını iste
    uint64_t control_block = pmm_alloc_page();
    if (control_block == 0) {
        print("Hata: Kontrol blogu icin 4KB fiziksel bellek ayrilamadi!");
        return nullptr;
    }

    // Bellek alanını tamamen sıfırla (Özellikle AMD VMCB temiz bir temiz sayfa ister)
    uint64_t* clean_ptr = (uint64_t*)control_block;
    for (int i = 0; i < 512; i++) {
        clean_ptr[i] = 0;
    }

    print("Kontrol Blogu Adresi Ayrildi: ");
    print_hex(control_block);

    // 3. Mimariye Özel İşlemler (Divergence)
    if (vendor == 1) {
        print("Mimarî Mod: Intel (VMCS Hazirlaniyor)");

        // Intel Kuralı: IA32_VMX_BASIC MSR (0x480) üzerinden Revizyon ID oku ve bellek başına yaz
        uint64_t vmx_basic = v_rdmsr(0x480);
        uint32_t revision_id = (uint32_t)vmx_basic;

        uint32_t* vmcs_header = (uint32_t*)control_block;
        *vmcs_header = revision_id; // Doğrudan adrese yazım düzeltildi

        print("Intel VMCS Revizyon ID Yazildi: ");
        print_hex(revision_id);

        // VMCS'i işlemciye yükle (VMPTRLD)
        if (!asm_load_vmcs(control_block)) {
            print("Hata: Intel VMPTRLD komutu basarisiz! Reset tetiklenebilir.");
            pmm_free_page(control_block);
            return nullptr;
        }
        print("Intel VMPTRLD Basarili: VMCS Aktif.");
    }
    else if (vendor == 2) {
        print("Mimarî Mod: AMD (VMCB Hazirlaniyor)");

        // AMD Standartları: AMD mimarisinde VMCB alanı ilk aşamada önden bir MSR/ID doğrulaması istemez.
        // VMCB'nin asıl doldurulması ve işlemciye tanıtılması direkt VMRUN assembly komutu esnasında olur.
        // Bu yüzden 4KB temiz alanı ayırmak ve sıfırlamak AMD tarafı için şimdilik tamamen yeterlidir.

        print("AMD VMCB Sayfasi Basariyla Ayrildi ve Sifirlandi.");
    }
    else {
        print("Hata: Desteklenmeyen islemci mimarisi! vCPU olusturulamadi.");
        pmm_free_page(control_block);
        return nullptr;
    }

    // Ortak Nesne Modelini Doldur
    global_vcpu.control_block_phys = control_block;
    global_vcpu.cpu_vendor_type = vendor;
    global_vcpu.is_active = true;

    print("Sanal CPU Kontrol Yapisi Hazir.");
    return &global_vcpu;
}