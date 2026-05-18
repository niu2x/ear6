#include <ear6/ear6.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <gtest/gtest.h>
#include <openssl/md5.h>

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

    int r = ear6_load_data(ctx, rom, sizeof(rom), nullptr);
    ASSERT_EQ(r, 0);

    int step = ear6_step(ctx);
    EXPECT_EQ(step, 0);

    const uint8_t* fb = ear6_get_framebuffer(ctx);
    ASSERT_NE(fb, nullptr);
    EXPECT_EQ(ear6_get_frame_width(ctx), 256);
    EXPECT_EQ(ear6_get_frame_height(ctx), 240);

    ear6_destroy(ctx);
}

TEST(Ear6Load, LoadPathSuccess) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);

    uint8_t rom[16 + 0x4000];
    memset(rom, 0, sizeof(rom));
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = 1; rom[5] = 0;
    rom[16] = 0x4C; rom[17] = 0x00; rom[18] = 0x80;

    const char* tmp = "/tmp/test_ear6_load.nes";
    FILE* f = std::fopen(tmp, "wb");
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(std::fwrite(rom, 1, sizeof(rom), f), sizeof(rom));
    std::fclose(f);

    int r = ear6_load(ctx, tmp);
    ASSERT_EQ(r, 0);
    std::remove(tmp);

    int step = ear6_step(ctx);
    EXPECT_EQ(step, 0);
    ear6_destroy(ctx);
}

TEST(Ear6Load, LoadPathInvalid) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    int r = ear6_load(ctx, "/nonexistent/path.rom");
    ASSERT_EQ(r, -3);
    ear6_destroy(ctx);
}

TEST(Ear6Load, LoadDataWithNameHint) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);

    uint8_t rom[16 + 0x4000];
    memset(rom, 0, sizeof(rom));
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = 1; rom[5] = 0;
    rom[16] = 0x4C; rom[17] = 0x00; rom[18] = 0x80;

    int r = ear6_load_data(ctx, rom, sizeof(rom), "test_game.nes");
    ASSERT_EQ(r, 0);

    int step = ear6_step(ctx);
    EXPECT_EQ(step, 0);
    ear6_destroy(ctx);
}

TEST(Ear6Load, LoadWithNullPathIsSafe) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    int r = ear6_load(ctx, nullptr);
    ASSERT_EQ(r, -1);
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

// -----------------------------------------------------------------------
// Regression: Choplifter (J).nes — ear6 correct, Mesen2 has this bug
// -----------------------------------------------------------------------

static std::string ppm_md5(const uint8_t* rgba, int w, int h) {
    char hdr[64];
    int hdr_len = snprintf(hdr, sizeof(hdr), "P6\n%d %d\n255\n", w, h);
    size_t ppm_size = (size_t)hdr_len + (size_t)w * (size_t)h * 3;

    // Build PPM in memory, compute MD5 directly
    uint8_t* ppm = (uint8_t*)malloc(ppm_size);
    memcpy(ppm, hdr, hdr_len);
    for (int i = 0; i < w * h; i++) {
        ppm[hdr_len + i * 3 + 0] = rgba[i * 4 + 0];
        ppm[hdr_len + i * 3 + 1] = rgba[i * 4 + 1];
        ppm[hdr_len + i * 3 + 2] = rgba[i * 4 + 2];
    }

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(ppm, ppm_size, digest);
    free(ppm);

    char hex[33];
    for (int i = 0; i < 16; i++) {
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    return std::string(hex, 32);
}

TEST(ChoplifterRegression, Frame30) {
    std::string rom_path = std::string(EAR6_SOURCE_DIR) + "/assets/nes/rom/Choplifter (J).nes";

    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);

    int rc = ear6_load(ctx, rom_path.c_str());
    ASSERT_EQ(rc, 0) << "ear6_load failed for: " << rom_path;

    for (int i = 0; i < 30; i++) {
        ASSERT_EQ(ear6_step(ctx), 0);
    }

    const uint8_t* fb = ear6_get_framebuffer(ctx);
    ASSERT_NE(fb, nullptr);
    ASSERT_EQ(ear6_get_frame_width(ctx), 256);
    ASSERT_EQ(ear6_get_frame_height(ctx), 240);

    std::string hash = ppm_md5(fb, 256, 240);
    EXPECT_EQ(hash, "ad2d0044501aca97ef186404d2af1248");

    ear6_destroy(ctx);
}
