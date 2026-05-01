#include "ButtonMatrix.h"
#include "hardware/gpio.h"

void ButtonMatrix::init(uint8_t rowGpioFirst, uint8_t colGpioFirst,
                        uint8_t numRows, uint8_t numCols) {
    m_rowFirst = rowGpioFirst;
    m_colFirst = colGpioFirst;
    m_numRows  = numRows;
    m_numCols  = numCols;

    for (uint8_t r = 0; r < numRows; ++r) {
        gpio_init(rowGpioFirst + r);
        gpio_set_dir(rowGpioFirst + r, GPIO_OUT);
        gpio_put(rowGpioFirst + r, 1);
    }

    for (uint8_t c = 0; c < numCols; ++c) {
        gpio_init(colGpioFirst + c);
        gpio_set_dir(colGpioFirst + c, GPIO_IN);
        gpio_pull_up(colGpioFirst + c);
    }
}

uint64_t ButtonMatrix::scan(uint32_t nowMs) {
    uint64_t changed = 0;

    for (uint8_t r = 0; r < m_numRows; ++r) {
        gpio_put(m_rowFirst + r, 0);

        for (uint8_t c = 0; c < m_numCols; ++c) {
            uint8_t idx = r * m_numCols + c;
            if (idx >= kNumButtons) break;

            bool rawPressed = !gpio_get(m_colFirst + c);
            ButtonState& s  = m_states[idx];

            // Debounce: only register a change after kDebounceMs
            if (rawPressed != s.pressed) {
                if ((nowMs - s.lastChangeMs) >= kDebounceMs) {
                    s.lastChangeMs  = nowMs;
                    s.pressed       = rawPressed;
                    s.justPressed   = rawPressed;
                    s.justReleased  = !rawPressed;
                    changed        |= (uint64_t(1) << idx);
                }
            } else {
                s.justPressed  = false;
                s.justReleased = false;
            }
        }

        gpio_put(m_rowFirst + r, 1);
    }

    return changed;
}

const ButtonState& ButtonMatrix::getState(uint8_t idx) const {
    static const ButtonState kEmpty{};
    if (idx >= kNumButtons) return kEmpty;
    return m_states[idx];
}
