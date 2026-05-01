#include "FaderScanner.h"
#include "EncoderScanner.h"
#include "ButtonMatrix.h"
#include "LedDriver.h"
#include "DisplayDriver.h"
#include "Protocol.h"
#include "Calibration.h"
#include "ModuleConfig.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "pico/time.h"

// ─── GPIO pin assignments (adjust to match PCB layout) ───────────────────────
static constexpr uint8_t kFaderGpioFirst   = 26; // ADC0–ADC3 on RP2040
static constexpr uint8_t kEncoderGpioFirst = 0;  // ENC0_A at GPIO0
static constexpr uint8_t kButtonRowFirst   = 16;
static constexpr uint8_t kButtonColFirst   = 20;
static constexpr uint8_t kLedDataPin       = 24;
static constexpr uint8_t kI2cSda           = 4;
static constexpr uint8_t kI2cScl           = 5;
static constexpr uint8_t kUartTx           = 8;
static constexpr uint8_t kUartRx           = 9;

// ─── Globals ─────────────────────────────────────────────────────────────────
static FaderScanner   g_faders;
static EncoderScanner g_encoders;
static ButtonMatrix   g_buttons;
static LedDriver      g_leds;
static DisplayDriver  g_displays;
static Calibration    g_calibration;

static uint32_t g_lastHeartbeatMs = 0;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void sendEvent(EventType type, uint16_t controlId, int16_t value) {
    HardwareEvent evt{};
    evt.magic     = kPacketMagic;
    evt.moduleId  = kModuleId;
    evt.type      = type;
    evt.controlId = controlId;
    evt.value     = value;
    evt.timestamp = to_ms_since_boot(get_absolute_time());
    protocolSendEvent(uart1, evt);
}

static void generateFaderEvents(uint16_t changed) {
    for (uint8_t i = 0; i < kNumFaders; ++i) {
        if (changed & (1u << i)) {
            sendEvent(EventType::FaderMove, i,
                      static_cast<int16_t>(g_faders.getValue(i)));
        }
    }
}

static void generateEncoderEvents(uint16_t changed) {
    for (uint8_t i = 0; i < kNumEncoders; ++i) {
        if (changed & (1u << i)) {
            int8_t delta = g_encoders.getDelta(i);
            sendEvent(EventType::EncoderDelta, i, delta);
        }
    }
}

static void generateButtonEvents(uint64_t changed) {
    for (uint8_t i = 0; i < kNumButtons; ++i) {
        if (changed & (uint64_t(1) << i)) {
            const auto& s = g_buttons.getState(i);
            sendEvent(s.pressed ? EventType::ButtonPress
                                : EventType::ButtonRelease,
                      i, s.pressed ? 1 : 0);
        }
    }
}

static void handleIncomingCommands() {
    uint8_t buf[sizeof(LedCommand)];
    if (!protocolReceiveCommand(uart1, buf, sizeof(buf))) return;

    auto cmd = static_cast<CommandType>(buf[2]);
    switch (cmd) {
        case CommandType::SetLed: {
            const auto* c = reinterpret_cast<const LedCommand*>(buf);
            g_leds.setColor(c->ledIndex, {c->r, c->g, c->b});
            break;
        }
        case CommandType::SetLedAll: {
            const auto* c = reinterpret_cast<const LedCommand*>(buf);
            g_leds.setAll({c->r, c->g, c->b});
            break;
        }
        case CommandType::SetBrightness:
            g_leds.setBrightness(buf[3]);
            break;
        case CommandType::RequestCalib:
            g_calibration.beginCapture();
            break;
        default:
            break;
    }
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {
    stdio_init_all();

    // UART to Pi
    uart_init(uart1, kUartBaudRate);
    gpio_set_function(kUartTx, GPIO_FUNC_UART);
    gpio_set_function(kUartRx, GPIO_FUNC_UART);

    // Hardware subsystems
    g_faders.init(kFaderGpioFirst);
    g_encoders.init(kEncoderGpioFirst);
    g_buttons.init(kButtonRowFirst, kButtonColFirst, 4, 8); // 4 rows × 8 cols = 32 buttons
    g_leds.init(kLedDataPin);
    g_displays.init(0, kI2cSda, kI2cScl);

    // Load calibration from flash (or use defaults).
    if (g_calibration.loadFromFlash()) {
        for (uint8_t i = 0; i < kNumFaders; ++i) {
            const auto& cal = g_calibration.get(i);
            g_faders.setCalibration(i, cal.rawMin, cal.rawMax);
        }
    }

    // Signal ready: brief green flash.
    g_leds.setAll({0, 32, 0});
    g_leds.flush();
    sleep_ms(200);
    g_leds.setAll({0, 0, 0});
    g_leds.flush();

    // ─── Main loop ────────────────────────────────────────────────────────
    while (true) {
        uint32_t nowMs = to_ms_since_boot(get_absolute_time());

        uint16_t faderChanged   = g_faders.scan();
        uint16_t encoderChanged = g_encoders.scan();
        uint64_t buttonChanged  = g_buttons.scan(nowMs);

        generateFaderEvents(faderChanged);
        generateEncoderEvents(encoderChanged);
        generateButtonEvents(buttonChanged);

        handleIncomingCommands();

        g_leds.flush();
        g_displays.flush();

        // Periodic heartbeat
        if (nowMs - g_lastHeartbeatMs >= kHeartbeatIntervalMs) {
            g_lastHeartbeatMs = nowMs;
            sendEvent(EventType::ModuleHeartbeat, 0, 0);
        }

        // Yield until next 1 ms tick
        sleep_until(make_timeout_time_ms(1));
    }
}
