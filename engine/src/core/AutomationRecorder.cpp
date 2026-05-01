#include "AutomationRecorder.h"

AutomationRecorder::AutomationRecorder() = default;

void AutomationRecorder::setMode(const std::string& controlId, AutomationMode mode) {
    m_modes[controlId] = mode;
}

AutomationMode AutomationRecorder::getMode(const std::string& controlId) const {
    auto it = m_modes.find(controlId);
    return (it != m_modes.end()) ? it->second : AutomationMode::Off;
}

void AutomationRecorder::startRecord(uint64_t startTick) {
    m_startTick = startTick;
    m_recording = true;
    m_pending.clear();
}

void AutomationRecorder::stopRecord(uint64_t endTick) {
    m_recording = false;

    if (m_callback) {
        for (auto& [id, lane] : m_pending) {
            m_callback(lane);
        }
    }
}

bool AutomationRecorder::isRecording() const { return m_recording; }

void AutomationRecorder::feed(const ControlEvent& evt) {
    if (!m_recording) return;

    auto mode = getMode(evt.controlId);
    if (mode != AutomationMode::Write && mode != AutomationMode::Overdub) return;

    auto& lane = m_pending[evt.controlId];
    lane.controlId = evt.controlId;

    AutomationPoint pt;
    pt.tick      = evt.timestampTicks - m_startTick;
    pt.controlId = evt.controlId;
    pt.value     = evt.value;

    if (mode == AutomationMode::Overdub) {
        // In overdub, only replace existing points within a 1-tick window.
        for (auto& existing : lane.points) {
            if (existing.tick == pt.tick) {
                existing.value = pt.value;
                return;
            }
        }
    }

    lane.points.push_back(pt);
}

AutomationLane AutomationRecorder::takeLane(const std::string& controlId) {
    auto it = m_pending.find(controlId);
    if (it == m_pending.end()) return {};
    AutomationLane lane = std::move(it->second);
    m_pending.erase(it);
    return lane;
}

void AutomationRecorder::onLaneReady(AutomationWriteCallback cb) {
    m_callback = std::move(cb);
}
