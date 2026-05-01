#include <gtest/gtest.h>
#include "../src/core/MidiRouter.h"

// Minimal stub so MidiRouter compiles without a real ALSA device.
// MidiOutput is a singleton; we override sendCC to capture calls.
#include "../src/midi/MidiOutput.h"

TEST(MidiRouter, JsonRoundTrip) {
    MidiRouter router;
    MidiTarget t;
    t.portId   = 0;
    t.channel  = 1;
    t.ccNumber = 7;
    t.minValue = 0;
    t.maxValue = 127;
    t.curve    = "linear";

    router.setTarget("ch1_fader", t);
    std::string json = router.saveMappingJson();
    EXPECT_FALSE(json.empty());

    MidiRouter router2;
    router2.loadMappingJson(json);
    auto targets = router2.getTargets("ch1_fader");
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].ccNumber, 7);
}

TEST(MidiRouter, RemoveTargets) {
    MidiRouter router;
    MidiTarget t;
    t.ccNumber = 74;
    router.setTarget("macro1", t);
    EXPECT_EQ(router.getTargets("macro1").size(), 1u);
    router.removeTargets("macro1");
    EXPECT_TRUE(router.getTargets("macro1").empty());
}
