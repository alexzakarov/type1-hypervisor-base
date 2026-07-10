#ifndef PMM_H
#define PMM_H

#include <cstdint>
#include <stddef.h> // size_t tanımı için
#include "multiboot.h"

// Sembolik Sabitler
#define PAGE_SIZE 4096
#define BITMAP_TYPE uint64_t
#define BITS_PER_ELEMENT (sizeof(BITMAP_TYPE) * 8)

/**
 * @brief Fiziksel Bellek Yöneticisini (PMM) başlatır.
 *
 * Multiboot üzerinden sistemdeki toplam RAM miktarını öğrenir,
 * kernel kodunun hemen bittiği güvenli adrese bir Bit Haritası (Bitmap) kurar.
 *
 * @param mb Multiboot bilgi yapısına işaret eden pointer.
 * @param kernel_end_addr Kernel kod ve verilerinin bittiği güvenli sınır adresi.
 */
void init_pmm(multiboot_info* mb, uint64_t kernel_end_addr);

/**
 * @brief Bellekten 4KB'lık boş bir fiziksel sayfa ayırır (Allocate).
 *
 * @return uint64_t Ayrılan sayfanın fiziksel başlangıç adresi. RAM'de yer kalmadıysa 0 döner.
 */
uint64_t pmm_alloc_page();

/**
 * @brief Daha önce ayrılmış olan bir fiziksel sayfayı serbest bırakır (Free).
 *
 * @param physical_address Serbest bırakılacak sayfanın 4KB hizalı fiziksel adresi.
 */
void pmm_free_page(uint64_t physical_address);

#endif // PMM_H
