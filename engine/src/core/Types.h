#pragma once
#include <cstdint>
#include <string>
#include <vector>

// ─── Fundamental MIDI / sequencer data types ─────────────────────────────────

static constexpr uint32_t kTicksPerBeat    = 960;
static constexpr uint32_t kTicksPerBar4_4  = kTicksPerBeat * 4; // 3840
static constexpr uint32_t kTicksPerSixteen = kTicksPerBeat / 4; // 240

struct MidiTarget {
    int         portId{0};
    int         channel{1};      // 1–16
    int         ccNumber{0};     // 0–127
    int         minValue{0};
    int         maxValue{127};
    std::string curve{"linear"}; // linear | exponential | logarithmic
    bool        invert{false};
    float       smoothing{0.0f}; // 0 = none, 1 = max
};

struct ControlEvent {
    std::string controlId;
    int         value;
    uint64_t    timestampTicks{0};
};

struct AutomationPoint {
    uint64_t    tick{0};
    std::string controlId;
    int         value{0};
};

struct AutomationLane {
    std::string                   controlId;
    std::vector<AutomationPoint>  points;
    bool                          loop{false};
};

struct AutomationClip {
    std::string                  clipId;
    uint64_t                     lengthTicks{kTicksPerBar4_4 * 4};
    bool                         loop{true};
    std::vector<AutomationLane>  lanes;
};

struct SceneValues {
    std::string              sceneId;
    std::string              name;
    // controlId → value map stored as parallel vectors for cache-friendliness.
    std::vector<std::string> controlIds;
    std::vector<int>         values;
};

enum class SceneRecallMode {
    Jump,   // instant
    Fade,   // smooth over duration_bars
    Drop,   // quantized: wait for next N-bar boundary then jump
    Morph,  // driven by external crossfader value
};
