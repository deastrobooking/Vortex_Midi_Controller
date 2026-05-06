#include <gtest/gtest.h>
#include "../src/core/LfoShape.h"
#include "../src/core/Types.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static LfoConfig makeCfg(LfoShapeType shape) {
    LfoConfig c;
    c.shape      = shape;
    c.bipolar    = false;
    c.depth      = 1.f;
    c.phaseOffset= 0.f;
    return c;
}

// ── Sine ──────────────────────────────────────────────────────────────────────

TEST(LfoShape, SineAtZeroPhase) {
    auto c = makeCfg(LfoShapeType::Sine);
    float v = LfoShape::evaluate(c, 0.f);
    // unipolar sine at 0 → near 0.5 (midpoint, rising from 0)
    EXPECT_NEAR(v, 0.5f, 0.01f);
}

TEST(LfoShape, SinePeakAtQuarterPhase) {
    auto c = makeCfg(LfoShapeType::Sine);
    float v = LfoShape::evaluate(c, 0.25f);
    EXPECT_NEAR(v, 1.0f, 0.01f);
}

TEST(LfoShape, SineValleyAtThreeQuarters) {
    auto c = makeCfg(LfoShapeType::Sine);
    float v = LfoShape::evaluate(c, 0.75f);
    EXPECT_NEAR(v, 0.0f, 0.01f);
}

// ── Square ────────────────────────────────────────────────────────────────────

TEST(LfoShape, SquareFirstHalfHigh) {
    auto c = makeCfg(LfoShapeType::Square);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.1f), 1.f, 0.01f);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.49f), 1.f, 0.01f);
}

TEST(LfoShape, SquareSecondHalfLow) {
    auto c = makeCfg(LfoShapeType::Square);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.51f), 0.f, 0.01f);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.99f), 0.f, 0.01f);
}

// ── Triangle ──────────────────────────────────────────────────────────────────

TEST(LfoShape, TrianglePeakAtHalf) {
    auto c = makeCfg(LfoShapeType::Triangle);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.5f), 1.f, 0.01f);
}

TEST(LfoShape, TriangleZeroAtEnds) {
    auto c = makeCfg(LfoShapeType::Triangle);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.f),  0.f, 0.01f);
    EXPECT_NEAR(LfoShape::evaluate(c, 1.0f), 0.f, 0.01f);
}

// ── Sawtooth ──────────────────────────────────────────────────────────────────

TEST(LfoShape, SawtoothRisesFromZeroToOne) {
    auto c = makeCfg(LfoShapeType::SawUp);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.f),   0.f, 0.01f);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.5f),  0.5f, 0.01f);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.99f), 0.99f, 0.02f);
}

TEST(LfoShape, SawDownFallsFromOneToZero) {
    auto c = makeCfg(LfoShapeType::SawDown);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.f),   1.f, 0.01f);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.5f),  0.5f, 0.01f);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.99f), 0.01f, 0.02f);
}

// ── Rhythmic gate envelope ────────────────────────────────────────────────────

TEST(LfoShape, Syncopated8thHasHighOnOddSlots) {
    auto c = makeCfg(LfoShapeType::Syncopated8th);
    // Odd slots (0-indexed): 1,3,5,7 → phases 0.125, 0.375, 0.625, 0.875
    // Attack region = first 7% of slot width (slot_w = 0.125)
    // Hold region = next 76% → mid of slot is well within hold
    // Slot 1 midpoint = 0.125 + 0.125/2 = 0.1875
    float v = LfoShape::evaluate(c, 0.1875f);
    EXPECT_GT(v, 0.5f) << "slot 1 should be HIGH (syncopated)";
}

TEST(LfoShape, Syncopated8thEvenSlotsLow) {
    auto c = makeCfg(LfoShapeType::Syncopated8th);
    // Even slot midpoint = slot 0 midpoint = 0.0625
    float v = LfoShape::evaluate(c, 0.0625f);
    EXPECT_LT(v, 0.5f) << "slot 0 should be LOW (on-beat)";
}

// ── Preview output ────────────────────────────────────────────────────────────

TEST(LfoShape, PreviewOutputCount) {
    auto c = makeCfg(LfoShapeType::Sine);
    auto samples = LfoShape::preview(c, 64);
    EXPECT_EQ(samples.size(), 64u);
}

TEST(LfoShape, PreviewAllInRange) {
    for (int shape = 0; shape < static_cast<int>(LfoShapeType::Custom); ++shape) {
        auto c = makeCfg(static_cast<LfoShapeType>(shape));
        c.bipolar = false;
        auto samples = LfoShape::preview(c, 32);
        for (float v : samples) {
            EXPECT_GE(v, 0.f)  << "shape=" << shape << " value=" << v;
            EXPECT_LE(v, 1.f)  << "shape=" << shape << " value=" << v;
        }
    }
}

// ── Custom shape interpolation ────────────────────────────────────────────────

TEST(LfoShape, CustomShapeInterpolatesTwoPoints) {
    LfoConfig c;
    c.shape  = LfoShapeType::Custom;
    c.bipolar = false;
    c.depth  = 1.f;
    c.customPoints = {{0.f, 0.f}, {1.f, 1.f}};
    EXPECT_NEAR(LfoShape::evaluate(c, 0.f),  0.f,  0.01f);
    EXPECT_NEAR(LfoShape::evaluate(c, 0.5f), 0.5f, 0.05f);
    EXPECT_NEAR(LfoShape::evaluate(c, 1.0f), 1.f,  0.01f);
}
