#include <gtest/gtest.h>
#include "../src/core/SnapshotEngine.h"

TEST(SnapshotEngine, JumpRecall) {
    SnapshotEngine se;
    se.registerControl("fader1", 0);
    se.registerControl("fader2", 0);

    SceneValues sv;
    sv.sceneId = "scene1";
    sv.name    = "Test";
    sv.controlIds = {"fader1", "fader2"};
    sv.values     = {100, 200};
    se.addScene(sv);

    std::vector<std::pair<std::string,int>> changes;
    se.onValueChange([&](const std::string& id, int v) {
        changes.push_back({id, v});
    });

    se.recallScene("scene1", SceneRecallMode::Jump);

    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(se.currentValues().at("fader1"), 100);
    EXPECT_EQ(se.currentValues().at("fader2"), 200);
}

TEST(SnapshotEngine, CaptureScene) {
    SnapshotEngine se;
    se.registerControl("enc1", 64);
    se.updateValue("enc1", 100);

    auto sv = se.captureScene("s2", "My Scene");
    EXPECT_EQ(sv.sceneId, "s2");
    ASSERT_EQ(sv.controlIds.size(), 1u);
    EXPECT_EQ(sv.values[0], 100);
}
