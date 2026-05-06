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
    if (!uart_is_readable(uart1)) return;

    // Peek at magic byte first.
    uint8_t magic = 0;
    uart_read_blocking(uart1, &magic, 1);
    if (magic != kPacketMagic) return;

    // Read moduleId and command type.
    uint8_t header[2];
    uart_read_blocking(uart1, header, 2);
    const auto cmd = static_cast<CommandType>(header[1]);

    switch (cmd) {
        case CommandType::SetLed: {
            // Remaining payload: ledIndex(2) + r,g,b(3) + checksum(1) = 6 bytes
            uint8_t payload[6];
            uart_read_blocking(uart1, payload, sizeof(payload));
            uint16_t ledIndex = static_cast<uint16_t>(payload[0] | (payload[1] << 8));
            g_leds.setColor(ledIndex, {payload[2], payload[3], payload[4]});
            break;
        }
        case CommandType::SetLedAll: {
            uint8_t payload[6];
            uart_read_blocking(uart1, payload, sizeof(payload));
            g_leds.setAll({payload[2], payload[3], payload[4]});
            break;
        }
        case CommandType::SetBrightness: {
            // payload: brightness(1) + checksum(1) = 2 bytes
            uint8_t payload[2];
            uart_read_blocking(uart1, payload, sizeof(payload));
            g_leds.setBrightness(payload[0]);
            break;
        }
        case CommandType::SetDisplayLabel: {
            // Header already consumed (magic, moduleId, type).
            // Next: displayIndex(1) + labelLen(1).
            uint8_t meta[2];
            uart_read_blocking(uart1, meta, 2);
            uint8_t displayIdx = meta[0];
            uint8_t labelLen   = meta[1];
            // Cap at 32 chars + 1 checksum byte.
            if (labelLen > 32) labelLen = 32;
            uint8_t labelBuf[33]{};
            uart_read_blocking(uart1, labelBuf, labelLen + 1); // +1 for checksum
            labelBuf[labelLen] = '\0';
            g_displays.setLabel(displayIdx,
                                reinterpret_cast<const char*>(labelBuf));
            break;
        }
        case CommandType::RequestCalib:
            g_calibration.beginCapture();
            // Consume checksum byte.
            { uint8_t cs; uart_read_blocking(uart1, &cs, 1); }
            break;
        case CommandType::RequestStatus:
            // Send a heartbeat immediately as a status response.
            sendEvent(EventType::ModuleHeartbeat, 0, 0);
            { uint8_t cs; uart_read_blocking(uart1, &cs, 1); }
            break;
        default:
            // Unknown command — consume up to 16 bytes to resync.
            for (int i = 0; i < 16 && uart_is_readable(uart1); ++i) {
                uint8_t discard; uart_read_blocking(uart1, &discard, 1);
            }
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
