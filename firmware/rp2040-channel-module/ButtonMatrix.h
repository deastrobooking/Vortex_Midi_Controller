#pragma once
#include <cstdint>
#include <array>
#include "ModuleConfig.h"

struct ButtonState {
    bool     pressed;
    bool     justPressed;
    bool     justReleased;
    uint32_t lastChangeMs;
};

class ButtonMatrix {
public:
    void init(uint8_t rowGpioFirst, uint8_t colGpioFirst,
              uint8_t numRows, uint8_t numCols);

    // Scan the matrix and update button states.
    // Returns bitmask of buttons that changed state.
    uint64_t scan(uint32_t nowMs);

    const ButtonState& getState(uint8_t idx) const;

private:
    uint8_t m_rowFirst{0};
    uint8_t m_colFirst{0};
    uint8_t m_numRows{0};
    uint8_t m_numCols{0};

    std::array<ButtonState, kNumButtons> m_states{};
    std::array<uint8_t,     kNumButtons> m_raw{};
};
