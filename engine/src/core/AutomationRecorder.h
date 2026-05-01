#pragma once
#include "Types.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

enum class AutomationMode {
    Off,
    Read,
    Write,
    Overdub,
};

using AutomationWriteCallback = std::function<void(const AutomationLane&)>;

class AutomationRecorder {
public:
    AutomationRecorder();

    void setMode(const std::string& controlId, AutomationMode mode);
    AutomationMode getMode(const std::string& controlId) const;

    // Call each time a control event arrives while the sequencer is playing.
    void feed(const ControlEvent& evt);

    // Start recording (arm).
    void startRecord(uint64_t startTick);

    // Stop recording and finalise lanes.
    void stopRecord(uint64_t endTick);

    bool isRecording() const;

    // Retrieve a completed lane for a control (empty if none recorded).
    AutomationLane takeLane(const std::string& controlId);

    // Called after stopRecord() to notify that a lane is ready.
    void onLaneReady(AutomationWriteCallback cb);

private:
    std::unordered_map<std::string, AutomationMode> m_modes;
    std::unordered_map<std::string, AutomationLane> m_pending;
    AutomationWriteCallback                          m_callback;
    uint64_t m_startTick{0};
    bool     m_recording{false};
};
