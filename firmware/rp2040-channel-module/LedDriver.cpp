#include "LedDriver.h"
#include "hardware/pio.h"
// WS2812 PIO program would normally be included here (ws2812.pio.h).
// For portability this file uses a placeholder transmit routine.

static void transmitByte(uint8_t byte) {
    // TODO: Replace with PIO-driven WS2812 bit-bang or DMA write.
    (void)byte;
}

void LedDriver::init(uint8_t dataPin) {
    m_dataPin = dataPin;
    // PIO / GPIO initialisation goes here.
}

void LedDriver::setColor(uint16_t idx, RgbColor color) {
    if (idx >= m_buffer.size()) return;
    m_buffer[idx] = color;
}

void LedDriver::setAll(RgbColor color) {
    for (auto& c : m_buffer) c = color;
}

void LedDriver::flush() {
    for (const auto& c : m_buffer) {
        uint8_t scale = m_brightness;
        transmitByte(static_cast<uint8_t>(c.g * scale / 255));
        transmitByte(static_cast<uint8_t>(c.r * scale / 255));
        transmitByte(static_cast<uint8_t>(c.b * scale / 255));
    }
    // WS2812 reset pulse (>50 µs low) handled by PIO/DMA timing.
}

void LedDriver::setBrightness(uint8_t brightness) {
    m_brightness = brightness;
}
