#include <gtest/gtest.h>

#include <cmath>

#include "character_motion.hpp"
#include "gameutil.hpp"

namespace {
class CharacterFade : public testing::Test {
protected:
    mgCFrame root;
    mgCFrame *frames[1]{&root};
    mgCVisualMDT visual;
    mgMaterial material{};
    u32 times[2]{0, 58};
    sceVu0FVECTOR values[2]{{0, 0, 0, 0}, {1, 0, 0, 0}};
    Mot_List channel{};
    tagMOTION_TYPE motion{};

    void SetUp() override {
        root.frame_num = 1;
        root.frame_list = frames;
        root.visual = &visual;
        visual.material = &material;
        visual.material_num = 1;
        channel.frame = channel.target = 0;
        channel.type = MOTION_KEY_MATERIAL_ALPHA;
        channel.key_count = 2;
        channel.key_frames = times;
        channel.values = values;
        motion.motion_list = &channel;
        character_motion::Register(&channel, {{"shatter", 58, 2, &channel}});
    }
    void TearDown() override { character_motion::Forget(&channel); }
    float Alpha(float frame) {
        character_motion::Pose(root, motion, frame, nullptr);
        return material.diffuse[3];
    }
};
}

TEST_F(CharacterFade, ShatterFadesAndRemainsTransparentAtAndAfterLastKey) {
    EXPECT_FLOAT_EQ(Alpha(2), 1);
    EXPECT_FLOAT_EQ(Alpha(31), .5f);
    EXPECT_FLOAT_EQ(Alpha(60), 0);
    EXPECT_FLOAT_EQ(Alpha(61), 0);
    EXPECT_FLOAT_EQ(Alpha(1000), 0);
    EXPECT_TRUE(std::isfinite(material.diffuse[3]));
}

TEST_F(CharacterFade, SingleKeyAndDelayedFirstKeyHoldTheirAuthoredOpacity) {
    channel.key_count = 1;
    times[0] = 10;
    values[0][0] = .75f;
    EXPECT_FLOAT_EQ(Alpha(2), .25f);
    EXPECT_FLOAT_EQ(Alpha(12), .25f);
    EXPECT_FLOAT_EQ(Alpha(60), .25f);
}

TEST_F(CharacterFade, BlendToLastKeyStaysFinite) {
    Alpha(31);
    character_motion::Blend(root, motion, 31, 60, .5f, nullptr);
    EXPECT_FLOAT_EQ(material.diffuse[3], .25f);
    character_motion::Blend(root, motion, 60, 60, 1, nullptr);
    EXPECT_FLOAT_EQ(material.diffuse[3], 0);
}

TEST_F(CharacterFade, CrossActionBlendAppliesTargetsAuthoredFadeUnblended) {
    Mot_List target = channel;
    sceVu0FVECTOR target_values[2]{{.25f, 0, 0, 0}, {.75f, 0, 0, 0}};
    target.values = target_values;
    character_motion::Register(&channel, {{"shatter", 58, 2, &channel}, {"other", 58, 100, &target}});
    Alpha(31);
    character_motion::Blend(root, motion, 31, 158, .2f, nullptr);
    EXPECT_FLOAT_EQ(material.diffuse[3], .25f);
}
