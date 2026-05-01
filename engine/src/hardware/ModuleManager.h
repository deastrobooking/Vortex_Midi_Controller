#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

// Binary protocol (matches firmware/rp2040-channel-module/Protocol.h)
static constexpr uint8_t kPktMagic = 0xAA;

enum class HWEventType : uint8_t {
    FaderMove       = 0x01,
    EncoderDelta    = 0x02,
    EncoderPress    = 0x03,
    ButtonPress     = 0x04,
    ButtonRelease   = 0x05,
    ModuleHeartbeat = 0x06,
    CalibrationData = 0x07,
};

struct HardwareEvent {
    uint8_t     moduleId;
    HWEventType type;
    uint16_t    controlId;
    int16_t     value;
    uint32_t    timestamp;
};

using HardwareEventCallback = std::function<void(const HardwareEvent&)>;

struct ModuleStatus {
    uint8_t  moduleId;
    bool     connected{false};
    uint32_t lastHeartbeatMs{0};
    double   eventRateHz{0.0};
};

class ModuleManager {
public:
    ModuleManager();
    ~ModuleManager();

    // Open serial port for a module (e.g. "/dev/ttyUSB0").
    bool connectModule(uint8_t moduleId, const std::string& serialPort);
    void disconnectModule(uint8_t moduleId);

    // Fire for every incoming hardware event across all modules.
    void onEvent(HardwareEventCallback cb);

    // Send LED color to a module.
    void sendLedColor(uint8_t moduleId, uint16_t ledIdx,
                      uint8_t r, uint8_t g, uint8_t b);

    // Send a display label to a module.
    void sendLabel(uint8_t moduleId, uint8_t displayIdx,
                   const std::string& label);

    std::vector<ModuleStatus> statuses() const;

private:
    void readerThread(uint8_t moduleId, int fd);

    struct Module {
        uint8_t     id;
        std::string port;
        int         fd{-1};
        std::thread reader;
        std::atomic<bool> active{false};
        ModuleStatus status;
        std::mutex   writeMtx;
    };

    std::vector<Module>      m_modules;
    HardwareEventCallback    m_callback;
    mutable std::mutex       m_mtx;
};
