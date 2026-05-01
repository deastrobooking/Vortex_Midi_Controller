#include "DisplayDriver.h"
#include "hardware/i2c.h"
#include <cstring>
#include <algorithm>

static constexpr uint8_t kSsd1306BaseAddr = 0x3C;

void DisplayDriver::init(uint8_t i2cInst, uint8_t sdaPin, uint8_t sclPin) {
    m_i2cInst = i2cInst;
    m_sdaPin  = sdaPin;
    m_sclPin  = sclPin;

    i2c_inst_t* inst = (i2cInst == 0) ? i2c0 : i2c1;
    i2c_init(inst, 400'000);
    gpio_set_function(sdaPin, GPIO_FUNC_I2C);
    gpio_set_function(sclPin, GPIO_FUNC_I2C);
    gpio_pull_up(sdaPin);
    gpio_pull_up(sclPin);

    // Initialise each display.
    for (uint8_t i = 0; i < kNumFaders; ++i) {
        uint8_t addr = kSsd1306BaseAddr + (i & 0x01); // TCA9548 mux or direct
        writeCommand(addr, 0xAE); // display off
        writeCommand(addr, 0x8D); writeCommand(addr, 0x14); // charge pump
        writeCommand(addr, 0xAF); // display on
    }
}

void DisplayDriver::setLabel(uint8_t idx, std::string_view label) {
    if (idx >= kNumFaders) return;
    size_t n = std::min(label.size(), size_t(8));
    std::memcpy(m_pages[idx].label, label.data(), n);
    m_pages[idx].label[n] = '\0';
    m_pages[idx].dirty = true;
}

void DisplayDriver::setValue(uint8_t idx, uint16_t value) {
    if (idx >= kNumFaders) return;
    m_pages[idx].value = value;
    m_pages[idx].dirty = true;
}

void DisplayDriver::flush() {
    for (uint8_t i = 0; i < kNumFaders; ++i) {
        if (!m_pages[i].dirty) continue;
        // TODO: render label + bar to a 128×32 framebuffer and DMA it.
        // Stub: mark clean.
        m_pages[i].dirty = false;
    }
}

void DisplayDriver::writeCommand(uint8_t i2cAddr, uint8_t cmd) {
    i2c_inst_t* inst = (m_i2cInst == 0) ? i2c0 : i2c1;
    uint8_t buf[2]   = {0x00, cmd};
    i2c_write_blocking(inst, i2cAddr, buf, 2, false);
}

void DisplayDriver::writeData(uint8_t i2cAddr, const uint8_t* data, size_t len) {
    i2c_inst_t* inst = (m_i2cInst == 0) ? i2c0 : i2c1;
    // Prepend data control byte 0x40.
    uint8_t header = 0x40;
    i2c_write_blocking(inst, i2cAddr, &header, 1, true);
    i2c_write_blocking(inst, i2cAddr, data, len, false);
}
