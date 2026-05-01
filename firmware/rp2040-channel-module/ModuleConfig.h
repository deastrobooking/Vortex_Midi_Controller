#pragma once
#include <cstdint>

// ─── Module identity ──────────────────────────────────────────────────────────
// Set MODULE_ID via compiler flag: -DMODULE_ID=0
#ifndef MODULE_ID
#define MODULE_ID 0
#endif

static constexpr uint8_t  kModuleId          = MODULE_ID;
static constexpr uint8_t  kFirmwareMajor      = 0;
static constexpr uint8_t  kFirmwareMinor      = 1;

// ─── Channel counts ───────────────────────────────────────────────────────────
static constexpr uint8_t  kNumFaders          = 16;
static constexpr uint8_t  kNumEncoders        = 16;
static constexpr uint8_t  kNumButtons         = 32;

// ─── ADC / fader ─────────────────────────────────────────────────────────────
static constexpr uint16_t kAdcResolution      = 4096;   // 12-bit
static constexpr uint16_t kFaderNoiseTolerance = 8;     // raw ADC counts
static constexpr uint32_t kFaderScanRateHz     = 500;

// ─── Encoder ─────────────────────────────────────────────────────────────────
static constexpr uint32_t kEncoderScanRateHz   = 1000;

// ─── Buttons ─────────────────────────────────────────────────────────────────
static constexpr uint32_t kButtonScanRateHz    = 1000;
static constexpr uint32_t kDebounceMs          = 5;

// ─── Display / LED ───────────────────────────────────────────────────────────
static constexpr uint32_t kLedUpdateRateHz     = 60;
static constexpr uint32_t kDisplayUpdateRateHz = 20;

// ─── Communication ───────────────────────────────────────────────────────────
static constexpr uint32_t kUartBaudRate        = 921600;
static constexpr uint32_t kHeartbeatIntervalMs = 500;
