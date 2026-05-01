#include "FaderScanner.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include <algorithm>
#include <cmath>

void FaderScanner::init(uint8_t gpioFirst) {
    m_gpioFirst = gpioFirst;
    adc_init();

    for (uint8_t i = 0; i < kNumFaders; ++i) {
        adc_gpio_init(gpioFirst + i);
        // Default calibration covers full ADC range.
        m_rawMin[i] = 0;
        m_rawMax[i] = kAdcResolution - 1;
    }
}

uint16_t FaderScanner::scan() {
    uint16_t changed = 0;

    for (uint8_t i = 0; i < kNumFaders; ++i) {
        adc_select_input(i);
        uint16_t raw = adc_read();
        m_current[i] = raw;

        int16_t delta = static_cast<int16_t>(raw) - static_cast<int16_t>(m_previous[i]);
        if (std::abs(delta) > static_cast<int16_t>(kFaderNoiseTolerance)) {
            m_previous[i] = raw;
            changed |= (1u << i);
        }
    }

    return changed;
}

uint16_t FaderScanner::getValue(uint8_t idx) const {
    if (idx >= kNumFaders) return 0;
    return mapToOutput(idx, m_current[idx]);
}

uint16_t FaderScanner::getRaw(uint8_t idx) const {
    if (idx >= kNumFaders) return 0;
    return m_current[idx];
}

void FaderScanner::setCalibration(uint8_t idx, uint16_t rawMin, uint16_t rawMax) {
    if (idx >= kNumFaders) return;
    m_rawMin[idx] = rawMin;
    m_rawMax[idx] = rawMax;
}

uint16_t FaderScanner::mapToOutput(uint8_t idx, uint16_t raw) const {
    uint16_t lo  = m_rawMin[idx];
    uint16_t hi  = m_rawMax[idx];
    if (hi == lo) return 0;
    uint32_t clamped = std::clamp(static_cast<uint32_t>(raw),
                                  static_cast<uint32_t>(lo),
                                  static_cast<uint32_t>(hi));
    return static_cast<uint16_t>((clamped - lo) * (kAdcResolution - 1) / (hi - lo));
}
