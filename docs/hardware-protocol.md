# Hardware Protocol — RP2040 Channel Module

**Source files:**
- `firmware/rp2040-channel-module/Protocol.h` / `Protocol.cpp`
- `engine/src/hardware/ModuleManager.h` / `ModuleManager.cpp`

---

## Overview

Each RP2040 channel module communicates with the engine daemon over a dedicated UART serial link at **921600 baud, 8N1**. Communication is bidirectional:

- **Module → Daemon** — hardware events (fader moves, button presses, encoder deltas, heartbeats, calibration data)
- **Daemon → Module** — LED commands, display labels, brightness control, calibration triggers

A magic byte `0xAA` starts every daemon→module packet. Module→daemon events have their own fixed-length format defined in `Protocol.h`.

---

## Hardware per Module

| Component | Count | Interface |
|---|---|---|
| Faders | 16 | 12-bit ADC (SPI or ADC mux) |
| Rotary encoders | 16 | 2-pin quadrature, interrupt-driven |
| Buttons | 32 | 4 × 8 scan matrix |
| WS2812B LEDs | 16 | PIO state machine |
| OLED displays | up to 4 | I2C (SSD1306 or SH1106) |

---

## Module → Daemon: Event Packets

All events start with a 1-byte `EventType` followed by type-specific payload, terminated by a 1-byte XOR checksum over the payload bytes.

| EventType | Payload | Total size |
|---|---|---|
| `FaderMove` | `moduleId[1]`, `faderId[1]`, `value[2]` (big-endian 12-bit) | 5 bytes |
| `ButtonPress` | `moduleId[1]`, `buttonId[1]`, `state[1]` (1=press, 0=release) | 4 bytes |
| `EncoderDelta` | `moduleId[1]`, `encoderId[1]`, `delta[1]` (signed) | 4 bytes |
| `CalibData` | `moduleId[1]`, `faderId[1]`, `min[2]`, `max[2]` | 7 bytes |
| `Heartbeat` | `moduleId[1]` | 2 bytes |

### Fader value scaling

The 12-bit raw ADC value is mapped to 0–16383 (14-bit) in the engine via the stored calibration min/max:

```
value_14bit = (raw - cal_min) * 16383 / (cal_max - cal_min)
```

Clamped to [0, 16383]. The `MidiRouter` then maps this to the configured MIDI CC range.

---

## Daemon → Module: Command Packets

Every command starts with magic byte `0xAA`, then a 2-byte header `[moduleId, commandType]`, followed by command-specific payload and a 1-byte XOR checksum over the payload.

### SetLed

```
0xAA | moduleId | 0x01 | ledIndex[2] | r | g | b | checksum
```

Sets a single WS2812B LED to RGB colour. `ledIndex` is little-endian 16-bit (maximum 65535 LEDs addressable, though each module has 16).

### SetLedAll

```
0xAA | moduleId | 0x02 | r | g | b | 0x00 | 0x00 | checksum
```

Sets all LEDs on the module to the same RGB colour.

### SetBrightness

```
0xAA | moduleId | 0x03 | brightness[1] | checksum
```

Global LED brightness scale for the module (0–255).

### SetDisplayLabel

```
0xAA | moduleId | 0x04 | displayIdx[1] | labelLen[1] | label[labelLen] | checksum
```

Writes a text label (max 32 bytes, UTF-8) to the specified OLED display. The firmware calls `g_displays.setLabel(displayIdx, label)`.

### RequestCalib

```
0xAA | moduleId | 0x05 | checksum
```

Instructs the module to begin a calibration capture cycle (user moves all faders through full range). The module responds with `CalibData` events per fader.

### RequestStatus

```
0xAA | moduleId | 0x06 | checksum
```

Module responds with a `Heartbeat` event.

---

## Checksum

The checksum byte is the XOR of all payload bytes (everything after the 2-byte header, excluding the checksum byte itself).

```cpp
uint8_t checksum = 0;
for (uint8_t b : payload) checksum ^= b;
```

The receiver recomputes and discards the packet if checksums don't match.

---

## Module Manager (Engine Side)

`ModuleManager` opens one reader thread per module serial port. It calls `onEvent(HardwareEvent)` on each parsed event. The event contains:

```cpp
struct HardwareEvent {
    uint8_t      moduleId;
    uint8_t      controlId;
    HWEventType  type;     // FaderMove, EncoderDelta, ButtonPress, ButtonRelease
    int          value;    // 14-bit for faders, ±1 for encoders, 0/1 for buttons
};
```

`main.cpp` registers a callback that translates `HardwareEvent` into a `ControlEvent` string ID (`m{moduleId}_{type}_{controlId}`) and routes it through `MidiRouter`.

---

## Adding a New Command Type

1. Add an enum value to `CommandType` in `Protocol.h`.
2. Add a `protocolSendXxx()` helper in `Protocol.cpp`.
3. Handle the new command in the firmware's `handleIncomingCommands()` switch statement in `main.cpp` — read magic byte, header, payload, checksum, call relevant driver.
4. Add a `ModuleManager::sendXxx()` method that calls `protocolSendXxx()` and writes to the serial fd.
