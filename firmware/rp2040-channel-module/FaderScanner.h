#pragma once
#include <cstdint>
#include <array>
#include "ModuleConfig.h"

class FaderScanner {
public:
    explicit FaderScanner() = default;

    // Call once during setup to configure ADC GPIO pins.
    // gpioFirst: first GPIO pin used for multiplexed ADC reads.
    void init(uint8_t gpioFirst);

    // Scan all faders. Returns a bitmask of channels that changed.
    // Updated values are accessible via getValue().
    uint16_t scan();

    // Returns the current calibrated value [0–4095] for channel idx.
    uint16_t getValue(uint8_t idx) const;

    // Returns the raw ADC value for channel idx (useful for calibration).
    uint16_t getRaw(uint8_t idx) const;

    // Store min/max calibration limits for a channel.
    void setCalibration(uint8_t idx, uint16_t rawMin, uint16_t rawMax);

private:
    uint16_t mapToOutput(uint8_t idx, uint16_t raw) const;

    std::array<uint16_t, kNumFaders> m_current{};
    std::array<uint16_t, kNumFaders> m_previous{};
    std::array<uint16_t, kNumFaders> m_rawMin{};
    std::array<uint16_t, kNumFaders> m_rawMax{};
    uint8_t m_gpioFirst{0};
};
