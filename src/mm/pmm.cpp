#include <cstdint>
#include "multiboot.h"
#include "pmm.h"
#include "utils.h"

// PMM Değişkenleri
BITMAP_TYPE* pmm_bitmap = nullptr;
uint64_t total_pages = 0;
uint64_t bitmap_size_elements = 0;

// ept.cpp içindeki extern bildirimiyle eşleşmesi için global tanımlıyoruz
extern "C" uint64_t slat_pml4_phys;

// Yardımcı Bit Fonksiyonları
static inline void bitmap_set(uint64_t page_index) {
    uint64_t array_index = page_index / BITS_PER_ELEMENT;
    uint64_t bit_index = page_index % BITS_PER_ELEMENT;
    pmm_bitmap[array_index] |= (1ULL << bit_index);
}

static inline void bitmap_clear(uint64_t page_index) {
    uint64_t array_index = page_index / BITS_PER_ELEMENT;
    uint64_t bit_index = page_index % BITS_PER_ELEMENT;
    pmm_bitmap[array_index] &= ~(1ULL << bit_index);
}

static inline bool bitmap_test(uint64_t page_index) {
    uint64_t array_index = page_index / BITS_PER_ELEMENT;
    uint64_t bit_index = page_index % BITS_PER_ELEMENT;
    return (pmm_bitmap[array_index] & (1ULL << bit_index)) != 0;
}

/**
 * PMM (Fiziksel Bellek Yöneticisi) Başlatıcı
 */
void init_pmm(multiboot_info* mb, uint64_t kernel_end_addr) {
    uint32_t* raw_mb_ptr = (uint32_t*)mb;

    // KESİN DÜZELTME 1: Multiboot standartlarına göre indeksler düzeltildi
    uint32_t flags             = raw_mb_ptr[0]; // Ofset 0
    uint32_t actual_mem_lower = raw_mb_ptr[1]; // Ofset 4 -> mem_lower
    uint32_t actual_mem_upper = raw_mb_ptr[2]; // Ofset 8 -> mem_upper

    print("Flags: ");
    print_int(flags);
    print("Okunan mem_lower: ");
    print_int(actual_mem_lower);
    print("Okunan mem_upper: ");
    print_int(actual_mem_upper);

    // Eğer veriler geçersizse güvenlik sınırı koy (Fallback)
    if (actual_mem_upper == 0) {
        print("Multiboot bellek verisi gecersiz! Statik 512MB RAM ataniyor.\n");
        uint64_t static_memory = 512ULL * 1024ULL * 1024ULL;
        total_pages = static_memory / PAGE_SIZE;
    } else {
        uint64_t total_memory = (actual_mem_lower + actual_mem_upper) * 1024;
        total_pages = total_memory / PAGE_SIZE;
    }

    // Matematiksel Hizalama
    bitmap_size_elements = (total_pages + BITS_PER_ELEMENT - 1) / BITS_PER_ELEMENT;
    uint64_t bitmap_size_bytes = bitmap_size_elements * sizeof(BITMAP_TYPE);

    print("Hesaplanan bitmap_size_elements: ");
    print_int(bitmap_size_elements);

    // KESİN DÜZELTME 2: PMM haritasının kendisini ve dağıtacağı ilk sayfayı
    // AMD donanımının şart koştuğu tam 4KB katına (Page Alignment) zorluyoruz.
    pmm_bitmap = (BITMAP_TYPE*)((kernel_end_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

    for (uint64_t i = 0; i < bitmap_size_elements; i++) {
        pmm_bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
    }

    uint64_t bitmap_end_addr = (uint64_t)pmm_bitmap + bitmap_size_bytes;
    uint64_t first_free_page = (bitmap_end_addr + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = first_free_page; i < total_pages; i++) {
        bitmap_clear(i);
    }

    print("PMM Modulu Basariyla Kuruldu.");
}

uint64_t pmm_alloc_page() {
    for (uint64_t i = 0; i < bitmap_size_elements; i++) {
        if (pmm_bitmap[i] == 0xFFFFFFFFFFFFFFFFULL) continue;

        for (int bit = 0; bit < BITS_PER_ELEMENT; bit++) {
            uint64_t page_idx = (i * BITS_PER_ELEMENT) + bit;
            if (!bitmap_test(page_idx)) {
                bitmap_set(page_idx);
                return page_idx * PAGE_SIZE; // Tam 4KB hizalı gerçek adresi döner
            }
        }
    }
    return 0;
}

void pmm_free_page(uint64_t physical_address) {
    uint64_t page_idx = physical_address / PAGE_SIZE;
    if (page_idx < total_pages) {
        bitmap_clear(page_idx);
    }
}
