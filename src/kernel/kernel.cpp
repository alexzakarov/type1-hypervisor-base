#include <cstdint>

#include "ept.h"
#include "multiboot.h"
#include "pmm.h"
#include "utils.h"
#include "vmcs_init.h"
#include "vmm.h"
#include "vmm_ext.h"
#include "vmm_loop.h"
#include "v_cpu.h"



// Assembly'deki irq0_handler_stub tarafından otomatik çağrılır (Zamanlayıcı)
extern "C" void c_irq0_handler(void) {
    //print("IRQ0 Tetiklendi: PIT Zamanlayici Aktif!");
}

// Assembly'deki irq1_handler_stub tarafından otomatik çağrılır (Klavye)
extern "C" void c_irq1_handler(void) {
    uint8_t status = inb(0x64);
    print("IRQ1 Tetiklendi: Klavye Aktif!");

    if (status & 0x01) {
        // 2. Ham veriyi oku (Buffer'ı boşaltır)
        uint8_t scancode = inb(0x60);

        // Tuş bırakma filtreleme (7. bit kontrolü)
        if (!(scancode & 0x80)) {
            // DÜZELTME 1: Türü 'const char*' yerine 'char' yapın
            const char* ascii = get_ascii_from_scancode(scancode);

            // DÜZELTME 2: Pointer kontrolü yerine karakter kontrolü (0 değilse) yapın
            if (ascii != nullptr) {
                // DÜZELTME 3: 'print' yerine sadece tek karakter basan 'print_char' kullanın!
                print_char(*ascii);
            }
        }
    }
}

extern "C" uint64_t end; // Linker script bitiş sembolü

// Kernel Giriş Noktası
extern "C" void kmain(multiboot_info* mb)
{
    // Ekranı temizlemek yerine doğrudan durumu yazdırıyoruz
    print("Mimariler Esitlendi: 64-bit Long Mode Aktif.");
    print("IDT ve PIC Basariyla Yuklendi. Kesmeler Dinleniyor...\n");
    print("Kernel 64-bit Modda Yuklendi.");

    if (mb != nullptr) {
        // PMM'i güvenli şekilde başlatıyoruz
        init_pmm(mb, reinterpret_cast<uint64_t>(&end));

        // 2. Host Sanal Bellek Yöneticisini (Sayfa Tablolarını) başlat
        init_vmm();

        // 2. Adım: Sanallaştırma Kilitlerini Aç (Aşama 2)
        bool vmx_activated = init_virtualization_extensions();

        if (vmx_activated) {

            print("Hypervisor Donanimi Aktif. VMXon Calisiyor.");

            // 3. Sanal İşlemci ve VMCS Kontrol Bloğunu Kur (Aşama 3 - Yeni Madde)
            VirtualCpu* vcpu = create_virtual_cpu();
            if (vcpu != nullptr && vcpu->is_active) {
                clear_screen();
                print("\n[TEBRiK] Asama 3 Basarili: VMCS Aktif Islemler Icin Hazir!");

                // 4. VMCS/VMCB Durum ve Kontrol Alanları (Aşama 3 - Bölüm 2)
                if (setup_vmcs_states(vcpu)) {

                    // 5. İKİNCİ SEVİYE ADRES DÖNÜŞÜMÜ (Aşama 4 - Yeni Madde)
                    if (setup_memory_virtualization(vcpu)) {
                        print("\n[MUKEMMEL] Asama 4 Basarili: EPT/NPT Bellek Donusumu Aktif!");
                        print("Sanal Makine tam donanimli izolatör korumasina alindi.");
                        start_virtual_machine_loop(vcpu);
                    } else {
                        print("Hata: Asama 4 (Bellek Sanallastirma) BASARISIZ!");
                    }
                }
            } else {
                print("\nHata: Asama 3 (VMCS Kurulumu) BASARISIZ!");
            }

        } else {
            print("Hypervisor Donanim Aktivasyonu BASARISIZ!");
        }

    }

    // İşlemci kesme döngüsünde arkada çalışırken ana kernel askıda kalır
    while (1) {
        asm volatile("hlt");
    }
}
// src/kernel.cpp dosyasının en altına (fonksiyonların dışına) ekleyin:
extern "C" void __stack_chk_fail(void) {
    print("Kritik Hata: Stack Smashing Algilandi!");
    while (1) {
        asm volatile("hlt");
    }
}

extern "C" void simulated_guest_kernel() {
    asm volatile(
        "sti\n"
        "lbl_hlt_loop:\n"
        "hlt\n"
        "jmp lbl_hlt_loop\n"
    );
}