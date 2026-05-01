#pragma once
#include <cstdint>
#include <array>
#include "ModuleConfig.h"

struct RgbColor { uint8_t r, g, b; };

class LedDriver {
public:
    // Configure the SPI/PIO pin used for WS2812-style LEDs.
    void init(uint8_t dataPin);

    // Set a single LED color (not yet committed to hardware).
    void setColor(uint16_t idx, RgbColor color);

    // Set all LEDs to the same color.
    void setAll(RgbColor color);

    // Push the current buffer to the hardware.
    void flush();

    void setBrightness(uint8_t brightness); // 0–255

private:
    uint8_t m_dataPin{0};
    uint8_t m_brightness{255};
    std::array<RgbColor, kNumFaders * 2> m_buffer{};  // 2 LEDs per channel
};
