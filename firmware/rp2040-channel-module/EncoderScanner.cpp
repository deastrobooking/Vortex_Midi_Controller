#include "EncoderScanner.h"
#include "hardware/gpio.h"
#include "pico/time.h"

// Quadrature lookup table: index = (prevState << 2 | currState), value = delta
static const int8_t kQuadTable[16] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
};

void EncoderScanner::init(uint8_t gpioFirst) {
    m_gpioFirst = gpioFirst;

    // Each encoder uses 3 GPIO pins: A, B, SW → 3 pins × 16 encoders
    for (uint8_t i = 0; i < kNumEncoders; ++i) {
        uint8_t base = gpioFirst + i * 3;
        gpio_init(base);     gpio_set_dir(base,     GPIO_IN); gpio_pull_up(base);
        gpio_init(base + 1); gpio_set_dir(base + 1, GPIO_IN); gpio_pull_up(base + 1);
        gpio_init(base + 2); gpio_set_dir(base + 2, GPIO_IN); gpio_pull_up(base + 2);

        uint8_t a = !gpio_get(base);
        uint8_t b = !gpio_get(base + 1);
        m_state[i]    = (a << 1) | b;
        m_delta[i]    = 0;
        m_position[i] = 0;
        m_pressed[i]  = false;
    }
}

uint16_t EncoderScanner::scan() {
    uint16_t changed = 0;

    for (uint8_t i = 0; i < kNumEncoders; ++i) {
        uint8_t base = m_gpioFirst + i * 3;
        uint8_t a = !gpio_get(base);
        uint8_t b = !gpio_get(base + 1);
        uint8_t sw = !gpio_get(base + 2);

        uint8_t curr = (a << 1) | b;
        int8_t  d    = kQuadTable[(m_state[i] << 2) | curr];
        m_state[i]   = curr;

        if (d != 0) {
            m_delta[i]    += d;
            m_position[i] += d;
            changed        |= (1u << i);
        }

        // Simple press detection (no full debounce here – see ButtonMatrix for that).
        m_pressed[i] = (sw != 0);
    }

    return changed;
}

int8_t EncoderScanner::getDelta(uint8_t idx) {
    if (idx >= kNumEncoders) return 0;
    int8_t d    = m_delta[idx];
    m_delta[idx] = 0;
    return d;
}

int32_t EncoderScanner::getPosition(uint8_t idx) const {
    if (idx >= kNumEncoders) return 0;
    return m_position[idx];
}

bool EncoderScanner::wasPressed(uint8_t idx) const {
    if (idx >= kNumEncoders) return false;
    return m_pressed[idx];
}
