#pragma once
#include "Types.h"
#include "ScaleEngine.h"
#include <functional>
#include <mutex>
#include <vector>

class MidiOutput;

using SeqNoteOnCallback  = std::function<void(uint8_t trackId, uint8_t note, uint8_t vel)>;
using SeqNoteOffCallback = std::function<void(uint8_t trackId, uint8_t note)>;
using SeqStepCallback    = std::function<void(uint8_t trackId, uint8_t stepIdx)>;

// 64-channel step + melodic sequencer driven by the clock.
// Polymetric: each track has independent step count, step length, and swing.
// Thread-safe: advance() is called from clock thread; all setter methods
// acquire the mutex so they may be called from the WebSocket thread.
class SequencerEngine {
public:
    explicit SequencerEngine(MidiOutput& midi, ScaleEngine& scales);

    // ── Pattern management ────────────────────────────────────────────────
    void loadPattern(SeqPattern pattern);
    void clearPattern();
    const SeqPattern& pattern() const;

    // ── Per-track configuration ───────────────────────────────────────────
    void setTrack(const SeqTrack& track);
    void setStep(uint8_t trackId, uint8_t stepIdx, const SeqStep& step);
    void setStepCount(uint8_t trackId, uint8_t count);     // 1–64
    void setStepTicks(uint8_t trackId, uint32_t ticks);
    void muteTrack(uint8_t trackId, bool muted);
    void soloTrack(uint8_t trackId, bool solo);
    void transposeTrack(uint8_t trackId, int8_t semitones);
    void setSwing(uint8_t trackId, float swing);           // 0.0–1.0

    // ── Mod-matrix hooks (called per tick by ModMatrix) ───────────────────
    void applyPitchMod(uint8_t trackId, float semitones);       // ±24 st
    void applyVelocityMod(uint8_t trackId, float normalised);   // −1 … +1
    void applyGateMod(uint8_t trackId, float scale);            // 0 … 2
    void applyRateMod(uint8_t trackId, float multiplier);       // 0.25 … 4

    // ── Clock-driven advance ──────────────────────────────────────────────
    void advance(uint64_t prevTick, uint64_t tick);

    // Reset all playheads (call on transport stop/start).
    void reset();

    // ── Callbacks for UI feedback ─────────────────────────────────────────
    void onNoteOn(SeqNoteOnCallback cb);
    void onNoteOff(SeqNoteOffCallback cb);
    void onStep(SeqStepCallback cb);    // fires on every step advance

    // Current playhead step per track (for UI).
    std::vector<uint8_t> currentSteps() const;

private:
    struct TrackState {
        uint8_t  currentStep{0};
        uint64_t nextStepTick{0};   // 0 = not yet initialised
        uint64_t noteOffTick{0};
        uint8_t  activeNote{255};   // 255 = none
        bool     noteActive{false};
        float    pitchMod{0.f};     // semitones
        float    velocityMod{0.f};  // normalised −1..+1
        float    gateMod{1.f};      // multiplier
        float    rateMod{1.f};      // step-rate multiplier
        bool     soloed{false};
    };

    void fireNoteOn(uint8_t trackId, uint8_t note, uint8_t velocity);
    void fireNoteOff(uint8_t trackId);

    // Returns the tick-adjusted step length (swing applied to odd steps).
    uint32_t stepLength(const SeqTrack& track, const TrackState& state) const;

    SeqPattern              m_pattern;
    std::vector<TrackState> m_states;   // always kMaxSeqTracks entries
    bool                    m_anySolo{false};

    MidiOutput&     m_midi;
    ScaleEngine&    m_scales;

    SeqNoteOnCallback  m_onNoteOn;
    SeqNoteOffCallback m_onNoteOff;
    SeqStepCallback    m_onStep;

    mutable std::mutex m_mtx;
};
