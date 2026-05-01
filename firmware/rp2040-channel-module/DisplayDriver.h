#pragma once
#include <cstdint>
#include <string_view>
#include "ModuleConfig.h"

// Simple SSD1306 I2C OLED driver (128×32 per channel strip).
class DisplayDriver {
public:
    void init(uint8_t i2cInst, uint8_t sdaPin, uint8_t sclPin);

    // Set the label shown on display idx (truncated to 8 chars).
    void setLabel(uint8_t idx, std::string_view label);

    // Show a value bar 0–4095 on display idx.
    void setValue(uint8_t idx, uint16_t value);

    // Commit pending changes to the OLED hardware.
    void flush();

private:
    void writeCommand(uint8_t i2cAddr, uint8_t cmd);
    void writeData(uint8_t i2cAddr, const uint8_t* data, size_t len);

    uint8_t m_i2cInst{0};
    uint8_t m_sdaPin{0};
    uint8_t m_sclPin{0};

    struct DisplayPage {
        char    label[9]{};
        uint16_t value{0};
        bool    dirty{false};
    };
    DisplayPage m_pages[kNumFaders]{};
};
