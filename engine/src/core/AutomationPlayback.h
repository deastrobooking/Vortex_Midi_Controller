#pragma once
#include "Types.h"
#include <functional>
#include <vector>

using PlaybackEventCallback = std::function<void(const ControlEvent&)>;

// Plays back automation clips in sync with the clock.
// The host calls advance() once per tick.
class AutomationPlayback {
public:
    AutomationPlayback();

    void loadClip(AutomationClip clip);
    void clearClip(const std::string& clipId);
    void clearAll();

    // Advance playback to the given absolute tick.
    // Fires the callback for every control event due in [prevTick, tick].
    void advance(uint64_t prevTick, uint64_t tick);

    void setLoop(bool loop);
    bool isLooping() const;

    // Called for every automation event that should be applied.
    void onEvent(PlaybackEventCallback cb);

private:
    // Returns interpolated value at fractional tick between two points.
    static int interpolate(const AutomationPoint& a,
                           const AutomationPoint& b,
                           uint64_t tick);

    std::vector<AutomationClip>  m_clips;
    PlaybackEventCallback        m_callback;
    bool                         m_loop{true};
};
