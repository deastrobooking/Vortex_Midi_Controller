#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// ─── Fundamental MIDI / sequencer data types ─────────────────────────────────

static constexpr uint32_t kTicksPerBeat    = 960;
static constexpr uint32_t kTicksPerBar4_4  = kTicksPerBeat * 4; // 3840
static constexpr uint32_t kTicksPerSixteen = kTicksPerBeat / 4; // 240
static constexpr uint32_t kTicksPerEighth  = kTicksPerBeat / 2; // 480
static constexpr uint32_t kTicksPerHalf    = kTicksPerBeat * 2; // 1920

static constexpr uint8_t  kMaxSeqTracks    = 64;

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

struct NoteEvent {
    uint8_t  midiPort{0};
    uint8_t  channel{1};     // 1–16
    uint8_t  note{60};
    uint8_t  velocity{100};
    uint64_t timestampTicks{0};
    bool     noteOn{true};
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

// ─── Scale ───────────────────────────────────────────────────────────────────

enum class ScaleMode : uint8_t {
    Major = 0,
    NaturalMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    HarmonicMinor,
    MelodicMinor,
    PentatonicMajor,
    PentatonicMinor,
    Blues,
    WholeTone,
    Diminished,   // half-whole alternating
    Augmented,
    Chromatic,
    Custom,
};

struct Scale {
    std::string             scaleId;
    std::string             name;
    ScaleMode               mode{ScaleMode::Major};
    int                     root{0};               // 0=C … 11=B
    std::array<bool, 12>    degrees{};             // active semitones
};

// ─── Step Sequencer ──────────────────────────────────────────────────────────

struct SeqStep {
    bool    active{false};
    uint8_t note{60};           // MIDI note (melodic mode); 0–127
    uint8_t velocity{100};      // 0–127
    uint8_t gatePercent{75};    // gate as % of step length (1–100)
    bool    accent{false};      // adds +20 velocity (clamped to 127)
    bool    slide{false};       // legato hint: suppress note-off before next on
    int8_t  pitchOffset{0};     // per-step semitone nudge (−24 … +24)
};

struct SeqTrack {
    uint8_t     trackId{0};
    std::string name;
    uint8_t     midiPort{0};
    uint8_t     midiChannel{1};     // 1–16
    uint8_t     baseNote{60};       // root note for step (non-melodic) mode
    bool        melodic{false};     // true: each step has its own note
    uint8_t     stepCount{16};      // 1–64
    uint32_t    stepTicks{kTicksPerSixteen}; // ticks per step
    std::vector<SeqStep> steps;
    bool        active{true};
    bool        muted{false};
    int8_t      transpose{0};       // global transpose in semitones
    bool        useScale{false};    // quantize notes to a scale
    std::string scaleId;
    float       swing{0.f};         // 0=straight, 1=max shuffle (delays odd steps)
};

struct SeqPattern {
    std::string            patternId;
    std::string            name;
    uint32_t               lengthBars{1};
    std::vector<SeqTrack>  tracks;  // indexed 0–63 by trackId
};

// ─── LFO ─────────────────────────────────────────────────────────────────────

enum class LfoShapeType : uint8_t {
    Sine            = 0,  // classic sine
    Triangle        = 1,  // linear ramp up then down
    Square50        = 2,  // 50% duty square ±1
    SawUp           = 3,  // rising sawtooth
    SawDown         = 4,  // falling sawtooth
    SampleHold      = 5,  // random value held per cycle (stepped)
    SmoothRandom    = 6,  // linearly interpolated random
    ExpRise         = 7,  // exponential acceleration
    ExpFall         = 8,  // exponential deceleration
    Pulse25         = 9,  // 25% duty square
    Pulse75         = 10, // 75% duty square
    StaircaseUp     = 11, // 8-step rising staircase
    StaircaseDown   = 12, // 8-step falling staircase
    Bounce          = 13, // |sin| — double-frequency positive bounce
    SineSquared     = 14, // sin² mapped to bipolar — sharp peaks
    Trapezoid       = 15, // ramp-up, hold-high, ramp-down, hold-low
    Syncopated8th   = 16, // rhythmic off-beat gate — 8 divisions, accents on 2,4,6,8
    Syncopated16th  = 17, // clave-like 16-slot gate pattern
    DottedGroove    = 18, // 3+3+2 accents across 8 sixteenth slots per bar
    Shuffle16th     = 19, // swing: each 8th pair split 2:1 (long-short)
    Custom          = 20, // user-drawn points
};

struct LfoPoint {
    float phase{0.f};  // 0.0 – 1.0 (position within one cycle)
    float value{0.f};  // −1.0 – 1.0 (normalised amplitude)
};

struct LfoConfig {
    std::string  lfoId;
    std::string  name;
    LfoShapeType shape{LfoShapeType::Sine};
    bool         syncToTempo{true};
    float        rateHz{1.f};                    // used when !syncToTempo
    uint32_t     syncDivision{kTicksPerBar4_4};  // ticks per LFO cycle
    float        depth{1.f};                     // output scale 0.0–1.0
    float        phaseOffset{0.f};               // cycle start offset 0.0–1.0
    bool         bipolar{true};                  // true: −1..1, false: 0..1
    std::vector<LfoPoint> customPoints;          // shape==Custom only
};

// ─── Modulation Matrix ───────────────────────────────────────────────────────

enum class ModSource : uint8_t {
    Lfo         = 0,  // sourceId = lfoId
    Velocity    = 1,  // last note-on velocity (0–127 scaled to −1..1)
    Aftertouch  = 2,  // channel pressure
    ModWheel    = 3,  // CC 1
    PitchBend   = 4,  // −8192 … +8191 scaled to −1..1
    MidiCC      = 5,  // sourceId = "cc<N>" e.g. "cc74"
    SeqCvLane   = 6,  // sourceId = "<trackId>" (step value as mod source)
};

enum class ModDest : uint8_t {
    MidiCC        = 0, // destId = controlId  → additive CC mod
    SeqPitch      = 1, // destId = "<trackId>" → pitch transpose ±24 semitones
    SeqVelocity   = 2, // destId = "<trackId>" → velocity ±127
    SeqGate       = 3, // destId = "<trackId>" → gate scale 0–2×
    SeqRate       = 4, // destId = "<trackId>" → step-rate multiplier 0.25–4×
    LfoRate       = 5, // destId = lfoId
    LfoDepth      = 6, // destId = lfoId
    SnapshotMorph = 7, // destId = sceneId pair "sceneA:sceneB"
};

struct ModRoute {
    std::string  routeId;
    ModSource    source{ModSource::Lfo};
    std::string  sourceId;
    ModDest      dest{ModDest::MidiCC};
    std::string  destId;
    float        amount{1.f};    // −1.0 – 1.0 (negative = invert)
    float        offset{0.f};    // additive bias on the scaled output
    bool         enabled{true};
};
