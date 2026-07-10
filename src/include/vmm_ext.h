#ifndef VMM_EXT_H
#define VMM_EXT_H

#include <cstdint>

// İşlemci Üretici Türleri
enum CpuVendor {
    VENDOR_UNKNOWN = 0,
    VENDOR_INTEL,
    VENDOR_AMD
};

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief İşlemcinin sanallaştırma uzantılarını (VT-x / SVM) tespit eder ve kilitlerini açar.
     * @return true Başarıyla tespit edilip aktif edildiyse, false Desteklenmiyor veya kilitliyse.
     */
    bool init_virtualization_extensions();

#ifdef __cplusplus
}
#endif

#endif // VMM_EXT_H
