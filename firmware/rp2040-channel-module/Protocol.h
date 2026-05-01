#pragma once
#include <cstdint>

// ─── Binary event protocol between RP2040 module and Raspberry Pi ─────────────
//
// All multi-byte fields are little-endian.
// Every packet starts with a 1-byte magic header (0xAA) and ends with a 1-byte
// XOR checksum over bytes [magic … last payload byte].

static constexpr uint8_t kPacketMagic = 0xAA;

enum class EventType : uint8_t {
    FaderMove       = 0x01,
    EncoderDelta    = 0x02,
    EncoderPress    = 0x03,
    ButtonPress     = 0x04,
    ButtonRelease   = 0x05,
    ModuleHeartbeat = 0x06,
    CalibrationData = 0x07,
};

// Pi → Module command types
enum class CommandType : uint8_t {
    SetLed          = 0x10,
    SetLedAll       = 0x11,
    SetDisplayLabel = 0x12,
    SetBrightness   = 0x13,
    RequestCalib    = 0x14,
    RequestStatus   = 0x15,
};

// ─── Event packet (11 bytes total) ───────────────────────────────────────────
struct __attribute__((packed)) HardwareEvent {
    uint8_t  magic;       // 0xAA
    uint8_t  moduleId;
    EventType type;
    uint16_t controlId;   // fader/encoder/button index
    int16_t  value;       // fader: 0-4095, encoder: delta, button: 0/1
    uint32_t timestamp;   // milliseconds since module boot
    uint8_t  checksum;    // XOR of all preceding bytes
};

// ─── LED command packet (8 bytes) ────────────────────────────────────────────
struct __attribute__((packed)) LedCommand {
    uint8_t  magic;       // 0xAA
    uint8_t  moduleId;
    CommandType type;     // SetLed
    uint16_t ledIndex;
    uint8_t  r, g, b;
    uint8_t  checksum;
};

// ─── Display label command (variable length, max 32 char label) ──────────────
struct __attribute__((packed)) DisplayLabelCommand {
    uint8_t  magic;
    uint8_t  moduleId;
    CommandType type;     // SetDisplayLabel
    uint8_t  displayIndex;
    uint8_t  labelLen;
    // followed by labelLen bytes of ASCII text, then checksum
};
