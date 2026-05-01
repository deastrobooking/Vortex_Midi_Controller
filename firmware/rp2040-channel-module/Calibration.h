#pragma once
#include <cstdint>
#include <array>
#include "ModuleConfig.h"

// Calibration stores raw ADC min/max per fader so that extreme travel
// maps cleanly to 0–4095 output regardless of physical tolerance.

struct FaderCalibration {
    uint16_t rawMin{0};
    uint16_t rawMax{4095};
};

class Calibration {
public:
    // Enter calibration mode: capture extremes while user moves faders.
    void beginCapture();

    // Feed a raw ADC value for channel idx during capture.
    void feedRaw(uint8_t idx, uint16_t raw);

    // Finalise and store results. Returns false if any channel saw < 10 %  travel.
    bool finalize();

    // Persist calibration to flash (RP2040 last flash page).
    void saveToFlash();

    // Load calibration from flash. Returns false if no valid data found.
    bool loadFromFlash();

    const FaderCalibration& get(uint8_t idx) const;

private:
    std::array<FaderCalibration, kNumFaders> m_cal{};
    std::array<uint16_t, kNumFaders> m_captureMin{};
    std::array<uint16_t, kNumFaders> m_captureMax{};
    bool m_capturing{false};
};
