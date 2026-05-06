#include "SequencerEngine.h"
#include "../midi/MidiOutput.h"
#include <algorithm>
#include <cassert>

SequencerEngine::SequencerEngine(MidiOutput& midi, ScaleEngine& scales)
    : m_midi(midi), m_scales(scales)
{
    m_states.resize(kMaxSeqTracks);
    // Pre-populate pattern with 64 empty tracks.
    m_pattern.tracks.resize(kMaxSeqTracks);
    for (uint8_t i = 0; i < kMaxSeqTracks; ++i) {
        m_pattern.tracks[i].trackId = i;
        m_pattern.tracks[i].name   = "Track " + std::to_string(i + 1);
        m_pattern.tracks[i].steps.resize(16); // default 16 steps
    }
}

// ─── Pattern management ───────────────────────────────────────────────────────

void SequencerEngine::loadPattern(SeqPattern pattern) {
    std::lock_guard lock(m_mtx);
    m_pattern = std::move(pattern);
    // Ensure exactly kMaxSeqTracks entries.
    m_pattern.tracks.resize(kMaxSeqTracks);
    for (uint8_t i = 0; i < kMaxSeqTracks; ++i)
        m_pattern.tracks[i].trackId = i;
    // Reset all playheads.
    m_states.assign(kMaxSeqTracks, TrackState{});
    m_anySolo = false;
}

void SequencerEngine::clearPattern() {
    loadPattern(SeqPattern{});
}

const SeqPattern& SequencerEngine::pattern() const {
    return m_pattern;
}

// ─── Per-track configuration ──────────────────────────────────────────────────

void SequencerEngine::setTrack(const SeqTrack& track) {
    std::lock_guard lock(m_mtx);
    if (track.trackId >= kMaxSeqTracks) return;
    m_pattern.tracks[track.trackId] = track;
}

void SequencerEngine::setStep(uint8_t trackId, uint8_t stepIdx, const SeqStep& step) {
    std::lock_guard lock(m_mtx);
    if (trackId >= kMaxSeqTracks) return;
    auto& t = m_pattern.tracks[trackId];
    if (stepIdx >= t.steps.size()) t.steps.resize(stepIdx + 1);
    t.steps[stepIdx] = step;
}

void SequencerEngine::setStepCount(uint8_t trackId, uint8_t count) {
    std::lock_guard lock(m_mtx);
    if (trackId >= kMaxSeqTracks) return;
    count = std::max(uint8_t{1}, count);
    auto& t = m_pattern.tracks[trackId];
    t.stepCount = count;
    t.steps.resize(count);
}

void SequencerEngine::setStepTicks(uint8_t trackId, uint32_t ticks) {
    std::lock_guard lock(m_mtx);
    if (trackId >= kMaxSeqTracks || ticks == 0) return;
    m_pattern.tracks[trackId].stepTicks = ticks;
}

void SequencerEngine::muteTrack(uint8_t trackId, bool muted) {
    std::lock_guard lock(m_mtx);
    if (trackId >= kMaxSeqTracks) return;
    m_pattern.tracks[trackId].muted = muted;
}

void SequencerEngine::soloTrack(uint8_t trackId, bool solo) {
    std::lock_guard lock(m_mtx);
    if (trackId >= kMaxSeqTracks) return;
    m_states[trackId].soloed = solo;
    m_anySolo = std::any_of(m_states.begin(), m_states.end(),
                            [](const TrackState& s){ return s.soloed; });
}

void SequencerEngine::transposeTrack(uint8_t trackId, int8_t semitones) {
    std::lock_guard lock(m_mtx);
    if (trackId >= kMaxSeqTracks) return;
    m_pattern.tracks[trackId].transpose = semitones;
}

void SequencerEngine::setSwing(uint8_t trackId, float swing) {
    std::lock_guard lock(m_mtx);
    if (trackId >= kMaxSeqTracks) return;
    m_pattern.tracks[trackId].swing = std::clamp(swing, 0.f, 1.f);
}

// ─── Mod-matrix hooks ─────────────────────────────────────────────────────────

void SequencerEngine::applyPitchMod(uint8_t trackId, float semitones) {
    if (trackId >= kMaxSeqTracks) return;
    m_states[trackId].pitchMod = std::clamp(semitones, -24.f, 24.f);
}

void SequencerEngine::applyVelocityMod(uint8_t trackId, float norm) {
    if (trackId >= kMaxSeqTracks) return;
    m_states[trackId].velocityMod = std::clamp(norm, -1.f, 1.f);
}

void SequencerEngine::applyGateMod(uint8_t trackId, float scale) {
    if (trackId >= kMaxSeqTracks) return;
    m_states[trackId].gateMod = std::clamp(scale, 0.f, 2.f);
}

void SequencerEngine::applyRateMod(uint8_t trackId, float multiplier) {
    if (trackId >= kMaxSeqTracks) return;
    m_states[trackId].rateMod = std::clamp(multiplier, 0.25f, 4.f);
}

// ─── Step length (swing aware) ────────────────────────────────────────────────

uint32_t SequencerEngine::stepLength(const SeqTrack& track,
                                     const TrackState& state) const {
    auto base = static_cast<uint32_t>(track.stepTicks * state.rateMod);
    if (base == 0) base = 1;

    // Swing: odd-indexed steps are stretched; even steps compensate.
    // Maximum swing adds 33% (2/3 : 1/3 split of an 8th-note pair).
    if (track.swing > 0.f && (state.currentStep % 2) == 1)
        base = static_cast<uint32_t>(base * (1.f + track.swing * 0.333f));

    return base;
}

// ─── Clock-driven advance ─────────────────────────────────────────────────────

void SequencerEngine::advance(uint64_t prevTick, uint64_t tick) {
    std::lock_guard lock(m_mtx);

    for (uint8_t i = 0; i < kMaxSeqTracks; ++i) {
        auto& track = m_pattern.tracks[i];
        auto& state = m_states[i];

        if (!track.active) continue;

        // Solo: skip un-soloed tracks.
        if (m_anySolo && !state.soloed) continue;

        // Muted tracks still advance the playhead; they just don't emit notes.
        bool silent = track.muted;

        // Fire any pending note-off in this tick range.
        if (state.noteActive &&
            state.noteOffTick > prevTick && state.noteOffTick <= tick)
        {
            if (!silent) fireNoteOff(i);
            state.noteActive = false;
        }

        if (track.steps.empty() || track.stepCount == 0) continue;

        // Initialise playhead on first call after reset.
        if (state.nextStepTick == 0) state.nextStepTick = tick;

        // Advance through all steps due in [prevTick, tick].
        while (state.nextStepTick <= tick) {
            uint8_t si = state.currentStep % track.stepCount;
            if (si >= static_cast<uint8_t>(track.steps.size())) {
                // Step list shorter than stepCount — just advance.
                state.currentStep = (state.currentStep + 1) % track.stepCount;
                state.nextStepTick += stepLength(track, state);
                continue;
            }

            const SeqStep& step = track.steps[si];

            if (step.active && !silent) {
                // ── Resolve note ───────────────────────────────────────────
                int note = track.melodic ? step.note : track.baseNote;
                note += track.transpose + step.pitchOffset;
                note += static_cast<int>(state.pitchMod);

                if (track.useScale) {
                    const Scale* sc = m_scales.find(track.scaleId);
                    if (sc) note = ScaleEngine::quantize(note, *sc);
                }
                note = std::clamp(note, 0, 127);

                // ── Resolve velocity ───────────────────────────────────────
                int vel = step.velocity + (step.accent ? 20 : 0);
                vel += static_cast<int>(state.velocityMod * 127.f);
                vel = std::clamp(vel, 1, 127);

                // ── Resolve gate ───────────────────────────────────────────
                float gateRatio = (step.gatePercent / 100.f) * state.gateMod;
                gateRatio = std::clamp(gateRatio, 0.01f, 2.f);
                uint32_t sl = stepLength(track, state);
                uint64_t gateTicks = static_cast<uint64_t>(sl * gateRatio);

                // Handle slide: suppress note-off until next note-on.
                if (state.noteActive && !step.slide) fireNoteOff(i);

                uint8_t noteU = static_cast<uint8_t>(note);
                uint8_t velU  = static_cast<uint8_t>(vel);
                fireNoteOn(i, noteU, velU);
                state.activeNote  = noteU;
                state.noteActive  = true;
                state.noteOffTick = state.nextStepTick + gateTicks;
            }

            if (m_onStep) m_onStep(i, si);

            // Advance playhead.
            state.nextStepTick += stepLength(track, state);
            state.currentStep   = (state.currentStep + 1) % track.stepCount;
        }
    }
}

void SequencerEngine::reset() {
    std::lock_guard lock(m_mtx);
    for (uint8_t i = 0; i < kMaxSeqTracks; ++i) {
        auto& state = m_states[i];
        if (state.noteActive) fireNoteOff(i);
        state = TrackState{};
        state.gateMod = 1.f;
        state.rateMod = 1.f;
    }
}

// ─── Callbacks ────────────────────────────────────────────────────────────────

void SequencerEngine::onNoteOn(SeqNoteOnCallback cb)  { m_onNoteOn  = std::move(cb); }
void SequencerEngine::onNoteOff(SeqNoteOffCallback cb) { m_onNoteOff = std::move(cb); }
void SequencerEngine::onStep(SeqStepCallback cb)      { m_onStep    = std::move(cb); }

std::vector<uint8_t> SequencerEngine::currentSteps() const {
    std::lock_guard lock(m_mtx);
    std::vector<uint8_t> out(kMaxSeqTracks);
    for (uint8_t i = 0; i < kMaxSeqTracks; ++i)
        out[i] = m_states[i].currentStep;
    return out;
}

// ─── Internal MIDI dispatch ───────────────────────────────────────────────────

void SequencerEngine::fireNoteOn(uint8_t trackId, uint8_t note, uint8_t velocity) {
    const auto& t = m_pattern.tracks[trackId];
    m_midi.sendNoteOn(t.midiPort, t.midiChannel - 1, note, velocity);
    if (m_onNoteOn) m_onNoteOn(trackId, note, velocity);
}

void SequencerEngine::fireNoteOff(uint8_t trackId) {
    const auto& t     = m_pattern.tracks[trackId];
    auto& state       = m_states[trackId];
    if (state.activeNote > 127) return;
    m_midi.sendNoteOff(t.midiPort, t.midiChannel - 1, state.activeNote);
    if (m_onNoteOff) m_onNoteOff(trackId, state.activeNote);
}
