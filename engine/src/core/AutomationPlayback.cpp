#include "AutomationPlayback.h"
#include <algorithm>
#include <cmath>

AutomationPlayback::AutomationPlayback() = default;

void AutomationPlayback::loadClip(AutomationClip clip) {
    // Sort each lane's points by tick for binary-search playback.
    for (auto& lane : clip.lanes) {
        std::sort(lane.points.begin(), lane.points.end(),
                  [](const AutomationPoint& a, const AutomationPoint& b) {
                      return a.tick < b.tick;
                  });
    }
    m_clips.push_back(std::move(clip));
}

void AutomationPlayback::clearClip(const std::string& clipId) {
    m_clips.erase(
        std::remove_if(m_clips.begin(), m_clips.end(),
                       [&clipId](const AutomationClip& c) {
                           return c.clipId == clipId;
                       }),
        m_clips.end());
}

void AutomationPlayback::clearAll() { m_clips.clear(); }
void AutomationPlayback::setLoop(bool loop) { m_loop = loop; }
bool AutomationPlayback::isLooping() const { return m_loop; }
void AutomationPlayback::onEvent(PlaybackEventCallback cb) { m_callback = std::move(cb); }

void AutomationPlayback::advance(uint64_t prevTick, uint64_t tick) {
    if (!m_callback) return;

    for (const auto& clip : m_clips) {
        uint64_t len = clip.lengthTicks;
        if (len == 0) continue;

        // Map global ticks to clip-local ticks (with optional looping).
        uint64_t localPrev = clip.loop ? (prevTick % len) : prevTick;
        uint64_t localTick = clip.loop ? (tick      % len) : tick;

        // Handle loop wrap: if localTick < localPrev the clip wrapped.
        bool wrapped = clip.loop && (localTick < localPrev);

        for (const auto& lane : clip.lanes) {
            if (lane.points.empty()) continue;

            auto emit = [&](uint64_t from, uint64_t to) {
                for (const auto& pt : lane.points) {
                    if (pt.tick > from && pt.tick <= to) {
                        ControlEvent evt;
                        evt.controlId      = lane.controlId;
                        evt.value          = pt.value;
                        evt.timestampTicks = tick;
                        m_callback(evt);
                    }
                }
            };

            if (wrapped) {
                emit(localPrev, len - 1);
                emit(0, localTick);
            } else {
                emit(localPrev, localTick);
            }
        }
    }
}

int AutomationPlayback::interpolate(const AutomationPoint& a,
                                    const AutomationPoint& b,
                                    uint64_t tick) {
    if (b.tick == a.tick) return a.value;
    double t = static_cast<double>(tick - a.tick) /
               static_cast<double>(b.tick - a.tick);
    return static_cast<int>(std::round(a.value + t * (b.value - a.value)));
}
