#include "Calibration.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <algorithm>
#include <cstring>

// Store calibration in the last 4 KB page of flash.
static constexpr uint32_t kFlashCalibOffset =
    PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
static constexpr uint32_t kCalibMagic = 0xCAL1B0;

void Calibration::beginCapture() {
    for (uint8_t i = 0; i < kNumFaders; ++i) {
        m_captureMin[i] = 4095;
        m_captureMax[i] = 0;
    }
    m_capturing = true;
}

void Calibration::feedRaw(uint8_t idx, uint16_t raw) {
    if (!m_capturing || idx >= kNumFaders) return;
    m_captureMin[idx] = std::min(m_captureMin[idx], raw);
    m_captureMax[idx] = std::max(m_captureMax[idx], raw);
}

bool Calibration::finalize() {
    m_capturing = false;
    bool ok     = true;

    for (uint8_t i = 0; i < kNumFaders; ++i) {
        uint16_t travel = m_captureMax[i] - m_captureMin[i];
        if (travel < (kAdcResolution / 10)) {
            ok = false; // Less than 10 % travel — suspect
        }
        m_cal[i].rawMin = m_captureMin[i];
        m_cal[i].rawMax = m_captureMax[i];
    }

    return ok;
}

void Calibration::saveToFlash() {
    struct FlashLayout {
        uint32_t magic;
        FaderCalibration data[kNumFaders];
    };

    FlashLayout layout{};
    layout.magic = kCalibMagic;
    std::memcpy(layout.data, m_cal.data(),
                sizeof(FaderCalibration) * kNumFaders);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(kFlashCalibOffset, FLASH_SECTOR_SIZE);
    flash_range_program(kFlashCalibOffset,
                        reinterpret_cast<const uint8_t*>(&layout),
                        sizeof(FlashLayout));
    restore_interrupts(ints);
}

bool Calibration::loadFromFlash() {
    const auto* layout = reinterpret_cast<const struct {
        uint32_t magic;
        FaderCalibration data[kNumFaders];
    }*>(XIP_BASE + kFlashCalibOffset);

    if (layout->magic != kCalibMagic) return false;

    std::memcpy(m_cal.data(), layout->data,
                sizeof(FaderCalibration) * kNumFaders);
    return true;
}

const FaderCalibration& Calibration::get(uint8_t idx) const {
    static const FaderCalibration kDefault{};
    if (idx >= kNumFaders) return kDefault;
    return m_cal[idx];
}
