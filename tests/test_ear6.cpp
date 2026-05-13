#include <ear6/ear6.h>

#include <gtest/gtest.h>

TEST(Ear6Create, TestSystemSuccess) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ear6_destroy(ctx);
}

TEST(Ear6Create, NesSystemWorks) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);

    uint8_t rom[16 + 0x4000];
    memset(rom, 0, sizeof(rom));
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = 1; rom[5] = 0;
    rom[16] = 0x4C; rom[17] = 0x00; rom[18] = 0x80;

    int r = ear6_load(ctx, rom, sizeof(rom));
    ASSERT_EQ(r, 0);

    int step = ear6_step(ctx);
    EXPECT_EQ(step, 0);

    const uint8_t* fb = ear6_get_framebuffer(ctx);
    ASSERT_NE(fb, nullptr);
    EXPECT_EQ(ear6_get_frame_width(ctx), 256);
    EXPECT_EQ(ear6_get_frame_height(ctx), 240);

    ear6_destroy(ctx);
}

TEST(Ear6Create, FlashSystemNotImplemented) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_FLASH);
    EXPECT_EQ(ctx, nullptr);
}

TEST(Ear6Create, ResourceLeakRepeatedCreateDestroy) {
    for (int i = 0; i < 1000; ++i) {
        Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
        ASSERT_NE(ctx, nullptr);
        ear6_destroy(ctx);
    }
}

TEST(Ear6Create, DestroyNullIsSafe) {
    ear6_destroy(nullptr);
}

TEST(Ear6Test, TestFunction) {
    EXPECT_EQ(ear6_test(), 42);
}
