#include <gtest/gtest.h>
#include "../src/core/ModMatrix.h"
#include "../src/core/LfoEngine.h"
#include "../src/core/SequencerEngine.h"
#include "../src/core/ScaleEngine.h"
#include "../src/core/MidiRouter.h"
#include "../src/midi/MidiOutput.h"
#include "../src/core/Types.h"

class ModMatrixTest : public ::testing::Test {
protected:
    vortex::ScaleEngine scales;
    LfoEngine           lfos;
    SequencerEngine     seq{ MidiOutput::instance(), scales };
    MidiRouter          router;
    ModMatrix           mm{ lfos, seq, router };
};

// ── Add / list / remove routes ────────────────────────────────────────────────

TEST_F(ModMatrixTest, AddRouteAppearsInList) {
    ModRoute r;
    r.routeId = "r1";
    r.source  = ModSource::Velocity;
    r.dest    = ModDest::SeqVelocity;
    r.amount  = 0.5f;
    r.enabled = true;

    mm.addRoute(r);
    const auto& all = mm.allRoutes();
    auto it = std::find_if(all.begin(), all.end(),
                [](const ModRoute& x){ return x.routeId == "r1"; });
    ASSERT_NE(it, all.end());
    EXPECT_NEAR(it->amount, 0.5f, 0.001f);
}

TEST_F(ModMatrixTest, RemoveRoute) {
    ModRoute r;
    r.routeId = "r2";
    r.source  = ModSource::Aftertouch;
    r.dest    = ModDest::SeqGate;
    r.amount  = 1.f;
    r.enabled = true;

    mm.addRoute(r);
    mm.removeRoute("r2");

    const auto& all = mm.allRoutes();
    EXPECT_TRUE(std::none_of(all.begin(), all.end(),
        [](const ModRoute& x){ return x.routeId == "r2"; }));
}

TEST_F(ModMatrixTest, UpdateRouteAmount) {
    ModRoute r;
    r.routeId = "r3";
    r.source  = ModSource::Lfo;
    r.dest    = ModDest::MidiCC;
    r.amount  = 1.f;
    r.enabled = true;

    mm.addRoute(r);
    r.amount = 0.25f;
    mm.updateRoute(r);

    const auto& all = mm.allRoutes();
    auto it = std::find_if(all.begin(), all.end(),
                [](const ModRoute& x){ return x.routeId == "r3"; });
    ASSERT_NE(it, all.end());
    EXPECT_NEAR(it->amount, 0.25f, 0.001f);
}

// ── MIDI CC feed ──────────────────────────────────────────────────────────────

TEST_F(ModMatrixTest, FeedCcDoesNotCrash) {
    EXPECT_NO_THROW(mm.feedCC(0 /*channel*/, 1 /*cc*/, 64 /*value*/));
    EXPECT_NO_THROW(mm.feedCC(15, 127, 127));
}

// ── Morph callback ────────────────────────────────────────────────────────────

TEST_F(ModMatrixTest, MorphCallbackFires) {
    bool fired = false;
    mm.onMorph([&](const std::string&, float) { fired = true; });

    ModRoute r;
    r.routeId  = "morph1";
    r.source   = ModSource::Velocity;
    r.dest     = ModDest::SnapshotMorph;
    r.destId   = "sceneA:sceneB";
    r.amount   = 1.f;
    r.enabled  = true;
    mm.addRoute(r);

    mm.feedVelocity(0, 127);
    mm.process(0);

    EXPECT_TRUE(fired);
}
