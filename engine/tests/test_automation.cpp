#include <gtest/gtest.h>
#include "../src/core/AutomationRecorder.h"
#include "../src/core/AutomationPlayback.h"

TEST(AutomationRecorder, RecordsWriteMode) {
    AutomationRecorder rec;
    rec.setMode("ch1_fader", AutomationMode::Write);
    rec.startRecord(0);

    ControlEvent e{"ch1_fader", 64, 100};
    rec.feed(e);

    rec.stopRecord(1000);
    AutomationLane lane = rec.takeLane("ch1_fader");

    ASSERT_EQ(lane.points.size(), 1u);
    EXPECT_EQ(lane.points[0].value, 64);
    EXPECT_EQ(lane.points[0].tick,  100u);
}

TEST(AutomationRecorder, IgnoresOffMode) {
    AutomationRecorder rec;
    rec.startRecord(0);

    ControlEvent e{"ch1_fader", 127, 0};
    rec.feed(e); // no mode set → Off

    rec.stopRecord(100);
    AutomationLane lane = rec.takeLane("ch1_fader");
    EXPECT_TRUE(lane.points.empty());
}

TEST(AutomationPlayback, FiresEvents) {
    AutomationPlayback pb;

    AutomationClip clip;
    clip.clipId       = "clip1";
    clip.lengthTicks  = 1000;
    clip.loop         = false;

    AutomationLane lane;
    lane.controlId = "ch1_fader";
    AutomationPoint pt{500, "ch1_fader", 88};
    lane.points.push_back(pt);
    clip.lanes.push_back(lane);

    pb.loadClip(std::move(clip));

    int fired = 0;
    pb.onEvent([&](const ControlEvent& e) {
        EXPECT_EQ(e.value, 88);
        ++fired;
    });

    pb.advance(0,   499);
    EXPECT_EQ(fired, 0);
    pb.advance(499, 501);
    EXPECT_EQ(fired, 1);
}
