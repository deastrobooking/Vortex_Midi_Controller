#include <gtest/gtest.h>
#include "../src/core/ScaleEngine.h"

using namespace vortex;

// ── makeScale ─────────────────────────────────────────────────────────────────

TEST(ScaleEngine, MajorScaleHasCorrectDegrees) {
    auto s = ScaleEngine::makeScale(ScaleMode::Major, 0 /*C*/);
    // C major: C D E F G A B → semitones 0,2,4,5,7,9,11 are true
    EXPECT_TRUE(s.degrees[0]);   // C
    EXPECT_FALSE(s.degrees[1]);  // C#
    EXPECT_TRUE(s.degrees[2]);   // D
    EXPECT_FALSE(s.degrees[3]);  // D#
    EXPECT_TRUE(s.degrees[4]);   // E
    EXPECT_TRUE(s.degrees[5]);   // F
    EXPECT_FALSE(s.degrees[6]);  // F#
    EXPECT_TRUE(s.degrees[7]);   // G
    EXPECT_FALSE(s.degrees[8]);  // G#
    EXPECT_TRUE(s.degrees[9]);   // A
    EXPECT_FALSE(s.degrees[10]); // A#
    EXPECT_TRUE(s.degrees[11]);  // B
}

TEST(ScaleEngine, MinorPentatonicScale) {
    auto s = ScaleEngine::makeScale(ScaleMode::PentatonicMinor, 0);
    // A-minor pentatonic rooted at C: 0,3,5,7,10
    EXPECT_TRUE(s.degrees[0]);
    EXPECT_TRUE(s.degrees[3]);
    EXPECT_TRUE(s.degrees[5]);
    EXPECT_TRUE(s.degrees[7]);
    EXPECT_TRUE(s.degrees[10]);
    // non-scale degrees
    EXPECT_FALSE(s.degrees[1]);
    EXPECT_FALSE(s.degrees[2]);
}

TEST(ScaleEngine, RootTransposition) {
    auto c = ScaleEngine::makeScale(ScaleMode::Major, 0);
    auto d = ScaleEngine::makeScale(ScaleMode::Major, 2);
    // D major: D E F# G A B C# → semitones 2,4,6,7,9,11,1
    EXPECT_TRUE(d.degrees[2]);   // D
    EXPECT_TRUE(d.degrees[4]);   // E
    EXPECT_TRUE(d.degrees[6]);   // F#
    EXPECT_FALSE(d.degrees[5]);  // F (not in D major)
    EXPECT_FALSE(c.degrees[6]);  // F# not in C major
}

// ── quantize ─────────────────────────────────────────────────────────────────

TEST(ScaleEngine, QuantizeInScaleNoteUnchanged) {
    auto s = ScaleEngine::makeScale(ScaleMode::Major, 0);
    // MIDI 60 = C4 → in C major → unchanged
    EXPECT_EQ(ScaleEngine::quantize(60, s), 60);
}

TEST(ScaleEngine, QuantizeOutOfScaleSnapDown) {
    auto s = ScaleEngine::makeScale(ScaleMode::Major, 0);
    // MIDI 61 = C#4 → not in C major → nearest is C (60) or D (62)
    uint8_t q = ScaleEngine::quantize(61, s);
    EXPECT_TRUE(q == 60 || q == 62) << "quantized=" << (int)q;
}

TEST(ScaleEngine, QuantizeBoundary) {
    auto s = ScaleEngine::makeScale(ScaleMode::Major, 0);
    uint8_t q = ScaleEngine::quantize(0, s);
    EXPECT_EQ(q, 0); // C — already in scale
}

// ── addScale / find / removeScale ────────────────────────────────────────────

TEST(ScaleEngine, AddFindRemoveCustomScale) {
    ScaleEngine eng;
    Scale s;
    s.scaleId = "my_scale";
    s.name    = "Whole Tone";
    s.mode    = ScaleMode::Custom;
    s.root    = 0;
    s.degrees = {true,false,true,false,true,false,true,false,true,false,true,false};

    eng.addScale(s);
    auto* found = eng.find("my_scale");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "Whole Tone");
    EXPECT_TRUE(found->degrees[2]); // D

    eng.removeScale("my_scale");
    EXPECT_EQ(eng.find("my_scale"), nullptr);
}

TEST(ScaleEngine, AllScalesReturnsAll) {
    ScaleEngine eng;
    EXPECT_TRUE(eng.allScales().empty());
    Scale s;
    s.scaleId = "a"; s.name = "A";
    eng.addScale(s);
    s.scaleId = "b"; s.name = "B";
    eng.addScale(s);
    EXPECT_EQ(eng.allScales().size(), 2u);
}
