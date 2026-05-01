#pragma once
#include <cstdint>
#include <array>
#include "ModuleConfig.h"

class EncoderScanner {
public:
    void init(uint8_t gpioFirst);

    // Returns a bitmask of encoders that produced a delta this scan.
    // Call getDelta() to retrieve the signed delta for each encoder.
    uint16_t scan();

    // Returns the signed delta since the last time this encoder was read (+/-).
    int8_t getDelta(uint8_t idx);

    // Returns accumulated absolute position (wraps freely).
    int32_t getPosition(uint8_t idx) const;

    // Returns true if the encoder switch was pressed this scan.
    bool wasPressed(uint8_t idx) const;

private:
    // Quadrature state machine: 2 bits per encoder stored in m_state.
    uint8_t m_gpioFirst{0};
    std::array<uint8_t,  kNumEncoders> m_state{};
    std::array<int8_t,   kNumEncoders> m_delta{};
    std::array<int32_t,  kNumEncoders> m_position{};
    std::array<bool,     kNumEncoders> m_pressed{};
    std::array<uint32_t, kNumEncoders> m_pressTimestamp{};
};
