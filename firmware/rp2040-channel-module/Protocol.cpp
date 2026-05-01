#include "Protocol.h"
#include "hardware/uart.h"
#include <cstring>

// ─── Helpers ─────────────────────────────────────────────────────────────────

static uint8_t calcChecksum(const uint8_t* data, size_t len) {
    uint8_t xorSum = 0;
    for (size_t i = 0; i < len; ++i) xorSum ^= data[i];
    return xorSum;
}

// ─── Send a hardware event over UART ─────────────────────────────────────────

void protocolSendEvent(uart_inst_t* uart, const HardwareEvent& evt) {
    // The struct already contains the magic byte at position 0.
    uint8_t buf[sizeof(HardwareEvent)];
    std::memcpy(buf, &evt, sizeof(HardwareEvent));

    // Overwrite checksum field (last byte) with computed XOR.
    buf[sizeof(HardwareEvent) - 1] =
        calcChecksum(buf, sizeof(HardwareEvent) - 1);

    uart_write_blocking(uart, buf, sizeof(buf));
}

// ─── Receive a command from the Pi ───────────────────────────────────────────
// Returns true if a valid command was read into *out.

bool protocolReceiveCommand(uart_inst_t* uart, uint8_t* outBuf, size_t bufSize) {
    if (!uart_is_readable(uart)) return false;

    uint8_t magic = 0;
    uart_read_blocking(uart, &magic, 1);
    if (magic != kPacketMagic) return false;
    if (bufSize < 2) return false;

    outBuf[0] = magic;
    uart_read_blocking(uart, outBuf + 1, bufSize - 1);

    uint8_t expected = calcChecksum(outBuf, bufSize - 1);
    return outBuf[bufSize - 1] == expected;
}
