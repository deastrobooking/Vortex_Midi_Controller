#include <gtest/gtest.h>
#include "../src/core/SequencerEngine.h"
#include "../src/core/ScaleEngine.h"
#include "../src/midi/MidiOutput.h"
#include "../src/core/Types.h"

// Minimal MidiOutput stub (singleton relies on linking with real MidiOutput.cpp;
// these tests only need the object to exist — no ALSA calls are made because
// we never call openAll()).

class SequencerTest : public ::testing::Test {
protected:
    vortex::ScaleEngine  scales;
    SequencerEngine      seq{ MidiOutput::instance(), scales };
};

// ── Basic step toggle ─────────────────────────────────────────────────────────

TEST_F(SequencerTest, DefaultPatternHas64Tracks) {
    const auto& pat = seq.pattern();
    EXPECT_EQ(pat.tracks.size(), 64u);
}

TEST_F(SequencerTest, SetStepActivatesCell) {
    SeqStep step;
    step.active   = true;
    step.note     = 60;
    step.velocity = 100;
    step.gatePercent = 75;

    seq.setStep(0, 0, step);
    const auto& s = seq.pattern().tracks[0].steps[0];
    EXPECT_TRUE(s.active);
    EXPECT_EQ(s.note, 60);
}

TEST_F(SequencerTest, ClearTrackDeactivatesAllSteps) {
    SeqStep step;
    step.active = true;
    step.note   = 64;
    for (int i = 0; i < 16; ++i) seq.setStep(0, i, step);

    seq.clearTrack(0);
    for (int i = 0; i < 16; ++i)
        EXPECT_FALSE(seq.pattern().tracks[0].steps[i].active) << "step " << i;
}

// ── Mute / solo ───────────────────────────────────────────────────────────────

TEST_F(SequencerTest, MuteTrack) {
    seq.muteTrack(3, true);
    EXPECT_TRUE(seq.pattern().tracks[3].muted);
    seq.muteTrack(3, false);
    EXPECT_FALSE(seq.pattern().tracks[3].muted);
}

TEST_F(SequencerTest, SoloTrack) {
    seq.soloTrack(5, true);
    EXPECT_TRUE(seq.pattern().tracks[5].soloed);
    seq.soloTrack(5, false);
    EXPECT_FALSE(seq.pattern().tracks[5].soloed);
}

// ── Track properties ──────────────────────────────────────────────────────────

TEST_F(SequencerTest, SetTrackStepCount) {
    SeqTrack t = seq.pattern().tracks[0];
    t.stepCount = 32;
    seq.setTrack(0, t);
    EXPECT_EQ(seq.pattern().tracks[0].stepCount, 32);
}

TEST_F(SequencerTest, SetTrackName) {
    SeqTrack t = seq.pattern().tracks[1];
    t.name = "Kick";
    seq.setTrack(1, t);
    EXPECT_EQ(seq.pattern().tracks[1].name, "Kick");
}

// ── Mod inputs ────────────────────────────────────────────────────────────────

TEST_F(SequencerTest, PitchModClamped) {
    // Should not throw or crash with extreme mod values.
    EXPECT_NO_THROW(seq.setPitchMod(0, 127));
    EXPECT_NO_THROW(seq.setPitchMod(0, -127));
}

TEST_F(SequencerTest, RateModBounded) {
    EXPECT_NO_THROW(seq.setRateMod(0, 4.0f));
    EXPECT_NO_THROW(seq.setRateMod(0, 0.25f));
}
