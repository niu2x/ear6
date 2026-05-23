#include <ear6/ear6.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <gtest/gtest.h>
#include <openssl/evp.h>

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

    unsigned char digest[EVP_MAX_MD_SIZE] = {};
    unsigned int digest_len = 0;

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        free(ppm);
        return std::string();
    }

    if (EVP_DigestInit_ex(md_ctx, EVP_md5(), nullptr) != 1
        || EVP_DigestUpdate(md_ctx, ppm, ppm_size) != 1
        || EVP_DigestFinal_ex(md_ctx, digest, &digest_len) != 1
        || digest_len != 16) {
        EVP_MD_CTX_free(md_ctx);
        free(ppm);
        return std::string();
    }

    EVP_MD_CTX_free(md_ctx);
    free(ppm);

    char hex[33];
    for (int i = 0; i < 16; i++) {
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    return std::string(hex, 32);
}

TEST(ChoplifterRegression, Frame30) {
    std::string rom_path = std::string(EAR6_SOURCE_DIR) + "/assets/nes/rom/mapper_3/Choplifter (J).nes";
    FILE* rom_file = std::fopen(rom_path.c_str(), "rb");
    if (!rom_file) {
        GTEST_SKIP() << "Missing test ROM: " << rom_path;
    }
    std::fclose(rom_file);

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

TEST(Smb3Regression, Frame60) {
    std::string rom_path = std::string(EAR6_SOURCE_DIR) + "/assets/nes/rom/mapper_4/Super Mario Bros 3 (J).nes";
    FILE* rom_file = std::fopen(rom_path.c_str(), "rb");
    if (!rom_file) {
        GTEST_SKIP() << "Missing test ROM: " << rom_path;
    }
    std::fclose(rom_file);

    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);

    int rc = ear6_load(ctx, rom_path.c_str());
    ASSERT_EQ(rc, 0) << "ear6_load failed for: " << rom_path;

    for (int i = 0; i < 60; i++) {
        ASSERT_EQ(ear6_step(ctx), 0);
    }

    const uint8_t* fb = ear6_get_framebuffer(ctx);
    ASSERT_NE(fb, nullptr);
    ASSERT_EQ(ear6_get_frame_width(ctx), 256);
    ASSERT_EQ(ear6_get_frame_height(ctx), 240);

    std::string hash = ppm_md5(fb, 256, 240);
    EXPECT_EQ(hash, "c7fe4fa1b152edab3028fd62572209ea");

    ear6_destroy(ctx);
}

// -----------------------------------------------------------------------
// Regression: Mapper 0 ROMs — verified 100% pixel match vs mesen2
// -----------------------------------------------------------------------

struct Mapper0TestEntry {
    const char* filename;
    int frame;
    const char* expected_md5;
};

static const Mapper0TestEntry kMapper0Tests[] = {
    {"10-Yard Fight (J).nes", 30, "5086aa38864671f0ecbea69514ef7a6e"},
    {"10-Yard Fight (J).nes", 60, "5086aa38864671f0ecbea69514ef7a6e"},
    {"1942 (JU).nes", 30, "c3bd05818671bbcfb2d9bb98024a1cdc"},
    {"1942 (JU).nes", 60, "c3bd05818671bbcfb2d9bb98024a1cdc"},
    {"4 Nin Uchi Mahjong (J).nes", 30, "d92297e5e7459ffb0f51dbf61bd1249d"},
    {"4 Nin Uchi Mahjong (J).nes", 60, "663fb6edf81981e81abeedced5e8b1fa"},
    {"Antarctic Adventure (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Antarctic Adventure (J).nes", 60, "320daef3029e761409a25981d38e5f80"},
    {"Arkanoid (J).nes", 30, "7f359527d7d74f1ff30c023e9e4c9db0"},
    {"Arkanoid (J).nes", 60, "7f359527d7d74f1ff30c023e9e4c9db0"},
    {"Astro Robo Sasa (J).nes", 30, "41aeaa25141585ce4479d063666845f8"},
    {"Astro Robo Sasa (J).nes", 60, "41aeaa25141585ce4479d063666845f8"},
    {"B-Wings (Alt) (J).nes", 30, "ab27a7b4313aa4b4c525a1ee243d70f0"},
    {"B-Wings (Alt) (J).nes", 60, "4779bf7133d98422262543ee7af809de"},
    {"B-Wings (J).nes", 30, "ab27a7b4313aa4b4c525a1ee243d70f0"},
    {"B-Wings (J).nes", 60, "4779bf7133d98422262543ee7af809de"},
    {"Balloon Fight (JU).nes", 30, "d0b9f820b607f3cf066ffd2cbd796774"},
    {"Balloon Fight (JU).nes", 60, "99da70b9494eb66e90371f0d2b6c6e96"},
    {"Baltron (J).nes", 30, "afe7573c37376a5a5cf8d5773d758e74"},
    {"Baltron (J).nes", 60, "902fc711bcf5a82cb6de3d1b90b39d47"},
    {"Baseball (J).nes", 30, "5787c4f9f82e91a2f84c0e745ff0bd7d"},
    {"Baseball (J).nes", 60, "5787c4f9f82e91a2f84c0e745ff0bd7d"},
    {"Battle City (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Battle City (J).nes", 60, "30b1330efc554592eb8c77b7c098f6bb"},
    {"Binary Land (J).nes", 30, "d8197e1f502eb25cfaa5699f826666f8"},
    {"Binary Land (J).nes", 60, "d8197e1f502eb25cfaa5699f826666f8"},
    {"Bird Week (J).nes", 30, "3576f63e686f6af419cd2fd6aeb2bc81"},
    {"Bird Week (J).nes", 60, "3576f63e686f6af419cd2fd6aeb2bc81"},
    {"BOMBMAN.NES", 30, "60b560646b8700a5bd9cbede2c288428"},
    {"BOMBMAN.NES", 60, "60b560646b8700a5bd9cbede2c288428"},
    {"Bokosuka Wars (J).nes", 30, "85dc6e20c25abbe7fe0ee7e7787643b2"},
    {"Bokosuka Wars (J).nes", 60, "85dc6e20c25abbe7fe0ee7e7787643b2"},
    {"Bomber Man (J).nes", 30, "9c93f7d1988804dcc9670a4a43cd809a"},
    {"Bomber Man (J).nes", 60, "9c93f7d1988804dcc9670a4a43cd809a"},
    {"Burger Time (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Burger Time (J).nes", 60, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Chack 'n Pop (J).nes", 30, "04fb2a330f573723b6d1afeac889a346"},
    {"Chack 'n Pop (J).nes", 60, "04fb2a330f573723b6d1afeac889a346"},
    {"Challenger (J).nes", 30, "1138bf0b655a2681ffd146d72806aff5"},
    {"Challenger (J).nes", 60, "1138bf0b655a2681ffd146d72806aff5"},
    {"Championship Lode Runner (J).nes", 30, "65ee5b7be3e1885e62152c75b68975d1"},
    {"Championship Lode Runner (J).nes", 60, "65ee5b7be3e1885e62152c75b68975d1"},
    {"Choujikuu Yousai - Macross (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Choujikuu Yousai - Macross (J).nes", 60, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Circus Charlie (J).nes", 30, "04a75ece8549771335d877ad14238460"},
    {"Circus Charlie (J).nes", 60, "087fe2067e0d17475197555502e46bde"},
    {"Clu Clu Land (JU).nes", 30, "e8049dee57adf21e7e74462ca66d649d"},
    {"Clu Clu Land (JU).nes", 60, "e8049dee57adf21e7e74462ca66d649d"},
    {"Devil World (Alt) (J).nes", 30, "69e2f3fad5c038eb6a5a3aab810330e5"},
    {"Devil World (Alt) (J).nes", 60, "69e2f3fad5c038eb6a5a3aab810330e5"},
    {"Devil World (J).nes", 30, "69e2f3fad5c038eb6a5a3aab810330e5"},
    {"Devil World (J).nes", 60, "69e2f3fad5c038eb6a5a3aab810330e5"},
    {"Dig Dug (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Dig Dug (J).nes", 60, "7e503541a169f83bd6b1b140bda03429"},
    {"Donkey Kong (JU).nes", 30, "8272be383d429286576ad24050a21d38"},
    {"Donkey Kong (JU).nes", 60, "8272be383d429286576ad24050a21d38"},
    {"Donkey Kong 3 (JUE).nes", 30, "4d0666319576be4c6ab9b48f003c5429"},
    {"Donkey Kong 3 (JUE).nes", 60, "4d0666319576be4c6ab9b48f003c5429"},
    {"Donkey Kong Jr (JU).nes", 30, "afb456ca8b98b24d0e8c98dcfe576e46"},
    {"Donkey Kong Jr (JU).nes", 60, "afb456ca8b98b24d0e8c98dcfe576e46"},
    {"Door Door (J).nes", 30, "763dd1f8d6d276c8562dc8139ee5e01b"},
    {"Door Door (J).nes", 60, "c7f1c50710db94f555d8a9d5577d2ddd"},
    {"Dough Boy (J).nes", 30, "18972493103741c8b8c3c4f849251a5d"},
    {"Dough Boy (J).nes", 60, "18972493103741c8b8c3c4f849251a5d"},
    {"Duck Hunt (JUE).nes", 30, "188ed183941d85266a26afd9c40f1b23"},
    {"Duck Hunt (JUE).nes", 60, "188ed183941d85266a26afd9c40f1b23"},
    {"Eigo Asobi (J).nes", 30, "4c6709bbf7c1a8b082198d83bea21f89"},
    {"Eigo Asobi (J).nes", 60, "4c6709bbf7c1a8b082198d83bea21f89"},
    {"Elevator Action (J).nes", 30, "5d695523f1e57f33afc3c0e13026856a"},
    {"Elevator Action (J).nes", 60, "a48f60fb7f5155d6f7a6206625968689"},
    {"Exed Exes (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Exed Exes (J).nes", 60, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Exerion (J).nes", 30, "d6fc5ceb5cd8ac375dfb76615e7cb1e3"},
    {"Exerion (J).nes", 60, "d6fc5ceb5cd8ac375dfb76615e7cb1e3"},
    {"F-1 Race (J).nes", 30, "ae216ab37bc7e9c7e8898d557178546e"},
    {"F-1 Race (J).nes", 60, "6ad6061a00d39061ca9112c1587fa8c2"},
    {"Famicom Disk System BIOS ROM (J).nes", 30, "b16003d2fb502a1baec2dfaeaba795f4"},
    {"Famicom Disk System BIOS ROM (J).nes", 60, "32398af592060eac22b833802db4867e"},
    {"Family BASIC (Ver 3) (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Family BASIC (Ver 3) (J).nes", 60, "a7a19fafd920cfc84ea7de6cb4b78f1f"},
    {"Field Combat (J).nes", 30, "4cc43baa70f13587de3ebde4baded291"},
    {"Field Combat (J).nes", 60, "4cc43baa70f13587de3ebde4baded291"},
    {"Flappy (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Flappy (J).nes", 60, "817aed79b8a5be8573d9f7e0047c0f61"},
    {"Formation Z (J).nes", 30, "2e029dffd32882a12275f07afc10a90c"},
    {"Formation Z (J).nes", 60, "2e029dffd32882a12275f07afc10a90c"},
    {"Galaga (J).nes", 30, "12cca13ab3f6f62c11aafed97fa6b322"},
    {"Galaga (J).nes", 60, "155081714213b194b5f55e6e777834b4"},
    {"Galaxian (J).nes", 30, "3db95c97a77a3ed4003836b8974df373"},
    {"Galaxian (J).nes", 60, "f1a8e1a5d585cb5d8a9a9e6da84bfb0c"},
    {"Galg (J).nes", 30, "0c5129ee00eca48ceb7287afc689d85e"},
    {"Galg (J).nes", 60, "0c5129ee00eca48ceb7287afc689d85e"},
    {"Gomoku Narabe (J).nes", 30, "a5f7d7a9921d6686dcc8b98792fa5702"},
    {"Gomoku Narabe (J).nes", 60, "a5f7d7a9921d6686dcc8b98792fa5702"},
    {"MARIO.NES", 30, "0d33c24e2028b4071ae3129d264e9c38"},
    {"MARIO.NES", 60, "f8502cf947e592d5a95787196de55de6"},
    {"Maajan (J).nes", 30, "4a9e93941223d19b765796a1e8f50aab"},
    {"Maajan (J).nes", 60, "4a9e93941223d19b765796a1e8f50aab"},
    {"Sansuu Asobi (J).nes", 30, "87ceb6958c70588af984f1e43b7123c0"},
    {"Sansuu Asobi (J).nes", 60, "87ceb6958c70588af984f1e43b7123c0"},
    {"Super Mario Bros (J).nes", 30, "0d33c24e2028b4071ae3129d264e9c38"},
    {"Super Mario Bros (J).nes", 60, "48fac751620532321c7fcdfec914462e"},
    {"Volguard 2 (J).nes", 30, "a460c1427f045e1af529d2fafe575792"},
    {"Volguard 2 (J).nes", 60, "a460c1427f045e1af529d2fafe575792"},
    {"Yie Ar Kung-Fu (J).nes", 30, "4f5ed731e0c42ded8e97aa5e412e5811"},
    {"Yie Ar Kung-Fu (J).nes", 60, "4f5ed731e0c42ded8e97aa5e412e5811"},
    {"c31.nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"c31.nes", 60, "b2bec19f823e7dd519545151d0a10a9f"},
    {"excite.nes", 30, "93157ae580ad8198a849247f06780684"},
    {"excite.nes", 60, "93157ae580ad8198a849247f06780684"},
    {"iceclimb.nes", 30, "0b99d377aed3175168b4fc1fe60437cd"},
    {"iceclimb.nes", 60, "0b99d377aed3175168b4fc1fe60437cd"},
    {"kinnik.nes", 30, "7f301b1d1487604042c2219dedd46f15"},
    {"kinnik.nes", 60, "b385b6954016ba0695ad68518087b02c"},
    {"pacman.nes", 30, "7c763c060a8950184b386e7467e6e900"},
    {"pacman.nes", 60, "0a1584ee928fa71017a5d5d21393c17c"},
    {"red.nes", 30, "42fdb45bb540c6340aa6aab3503b35b1"},
    {"red.nes", 60, "42fdb45bb540c6340aa6aab3503b35b1"},
    {"roadfigh.nes", 30, "ee9d486feb4da597b871c6729e6cea4c"},
    {"roadfigh.nes", 60, "ee9d486feb4da597b871c6729e6cea4c"},
    {"wolfpig.nes", 30, "b575280ea4e9525f56029b3bc4230957"},
    {"wolfpig.nes", 60, "01131be40757e2e4b64393e7a62afaf2"},
};

class Mapper0RegressionTest : public ::testing::TestWithParam<Mapper0TestEntry> {};

TEST_P(Mapper0RegressionTest, Frame) {
    const auto& param = GetParam();
    std::string rom_path = std::string(EAR6_SOURCE_DIR) + "/assets/nes/rom/mapper_0/" + param.filename;
    FILE* rom_file = std::fopen(rom_path.c_str(), "rb");
    if (!rom_file) {
        GTEST_SKIP() << "Missing test ROM: " << rom_path;
    }
    std::fclose(rom_file);

    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);

    int rc = ear6_load(ctx, rom_path.c_str());
    ASSERT_EQ(rc, 0) << "ear6_load failed for: " << rom_path;

    for (int i = 0; i < param.frame; i++) {
        ASSERT_EQ(ear6_step(ctx), 0);
    }

    const uint8_t* fb = ear6_get_framebuffer(ctx);
    ASSERT_NE(fb, nullptr);
    ASSERT_EQ(ear6_get_frame_width(ctx), 256);
    ASSERT_EQ(ear6_get_frame_height(ctx), 240);

    std::string hash = ppm_md5(fb, 256, 240);
    EXPECT_EQ(hash, param.expected_md5);

    ear6_destroy(ctx);
}

INSTANTIATE_TEST_SUITE_P(Mapper0Regression, Mapper0RegressionTest,
    ::testing::ValuesIn(kMapper0Tests));

// -----------------------------------------------------------------------
// Regression: Mapper 2 ROMs — verified 100% pixel match vs mesen2
// -----------------------------------------------------------------------

struct Mapper2TestEntry {
    const char* filename;
    int frame;
    const char* expected_md5;
};

static const Mapper2TestEntry kMapper2Tests[] = {
    {"1943 (J).nes", 30, "965c77555160562a3483f22c06994bed"},
    {"1943 (J).nes", 60, "965c77555160562a3483f22c06994bed"},
    {"1944.nes", 30, "155a3d96f6955c993ba99177ab3d9a46"},
    {"1944.nes", 60, "155a3d96f6955c993ba99177ab3d9a46"},
    {"Aigiina No Yogen (J).nes", 30, "088990aa4a1af2ff87b8314a94b1bf9f"},
    {"Aigiina No Yogen (J).nes", 60, "f45ef4c7b77532014aac2ca7030a7e47"},
    {"Akumajou Dracula (J).nes", 30, "6e34b03fd62b7a62f687cbe2e0c49e51"},
    {"Akumajou Dracula (J).nes", 60, "6e34b03fd62b7a62f687cbe2e0c49e51"},
    {"Arctic (Trained) (J).nes", 30, "b73cbffb79ab04407dfe4867fc704d76"},
    {"Arctic (Trained) (J).nes", 60, "bebf26ce5906dfdc3f2e389c82eb2a81"},
    {"Argos No Senshi (J).nes", 30, "9aa8b5a651c60fd844629cc468727609"},
    {"Argos No Senshi (J).nes", 60, "9aa8b5a651c60fd844629cc468727609"},
    {"Athena (J).nes", 30, "04a75ece8549771335d877ad14238460"},
    {"Athena (J).nes", 60, "04a75ece8549771335d877ad14238460"},
    {"Attack Animal Gakuen (J).nes", 30, "897548b57c4bfa8c195e2c66a3b6d076"},
    {"Attack Animal Gakuen (J).nes", 60, "55f099cb44b741c8651032478d72c487"},
    {"Ballblazer (J).nes", 30, "6d0b49264a2fb511788d893803344a1f"},
    {"Ballblazer (J).nes", 60, "6d0b49264a2fb511788d893803344a1f"},
    {"Bat & Tery (J).nes", 30, "51ab1d6b18d89ef0963261b6b1e7fa4b"},
    {"Bat & Tery (J).nes", 60, "51ab1d6b18d89ef0963261b6b1e7fa4b"},
    {"Black Bass 2, The (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Black Bass 2, The (J).nes", 60, "e92ebb417bbd40324e934f4a67879813"},
    {"Black Bass, The (J).nes", 30, "e71759ab985a3d046c54a4af9b39a40b"},
    {"Black Bass, The (J).nes", 60, "e71759ab985a3d046c54a4af9b39a40b"},
    {"Bomber King (J).nes", 30, "47bd5b932e45f27aade7f359a823246b"},
    {"Bomber King (J).nes", 60, "36e5dcacf7a341e2b4f754bf818405b9"},
    {"Booby Kids (J).nes", 30, "ac09475740e7d08519f3d5229befc90d"},
    {"Booby Kids (J).nes", 60, "f465624ede13a28fe0babd9b18a411cf"},
    {"CONTRA.NES", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"CONTRA.NES", 60, "d09226018aea34fa8b4e5b18645a6d54"},
    {"Chester Field (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Chester Field (J).nes", 60, "862e47e1fa18b006b5ccb830e0fcadf3"},
    {"City Adventure Touch - Mystery of Triangle (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"City Adventure Touch - Mystery of Triangle (J).nes", 60, "cf5c5ff145139dcd2ffec86eec506678"},
    {"Commando (J).nes", 30, "aeb437756d95e9244e57402f679d6cba"},
    {"Commando (J).nes", 60, "aeb437756d95e9244e57402f679d6cba"},
    {"Daiva - Imperial of Nirsartia (J).nes", 30, "c8915d839e980095bb6eb8896230643f"},
    {"Daiva - Imperial of Nirsartia (J).nes", 60, "555efd9b6d1f775806a52f639891f29b"},
    {"Dragon Quest 2 (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Dragon Quest 2 (J).nes", 60, "1c97d38520e9ede727a8a9b19df69933"},
    {"Dragon Quest 3 (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Dragon Quest 3 (J).nes", 60, "67242f79197c9e7df88704c2ccc5ac96"},
    {"Dragon Unit (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Dragon Unit (J).nes", 60, "40a9f161a1494894fc744bc69217c7ae"},
    {"Duck Tales 2 (J).nes", 30, "3b2518d191dbc2c3f241a4d51e2abf29"},
    {"Duck Tales 2 (J).nes", 60, "3b2518d191dbc2c3f241a4d51e2abf29"},
    {"Ernarc No Zaihou (J).nes", 30, "c84d66c3b3214cea4b8bd20960214f09"},
    {"Ernarc No Zaihou (J).nes", 60, "304ab55c88527637e86eb4146087f84a"},
    {"Esper Bouken Tai (J).nes", 30, "58c18263cdad56593c3c78eb7ca831f3"},
    {"Esper Bouken Tai (J).nes", 60, "58c18263cdad56593c3c78eb7ca831f3"},
    {"Flying Hero (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Flying Hero (J).nes", 60, "83c65b414ac1a30bb8c56f3d20f3ba70"},
    {"Foton (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Foton (J).nes", 60, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"GUNSMOKE.NES", 30, "27e59ab79afdc13481761edf35a5f394"},
    {"GUNSMOKE.NES", 60, "e87069baac22cfe155995c9354d3af13"},
    {"Ikari (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Ikari (J).nes", 60, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Jackal (U).nes", 30, "320e09f46b510f202f952727ea5bdd65"},
    {"Jackal (U).nes", 60, "320e09f46b510f202f952727ea5bdd65"},
    {"Makaimura (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Makaimura (J).nes", 60, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Moero Twinbee (J).nes", 30, "3031c641fcdc7e87e47c7ca9df85cdd7"},
    {"Moero Twinbee (J).nes", 60, "3031c641fcdc7e87e47c7ca9df85cdd7"},
    {"Nekketsu Kouha - Kunio Kun (J).nes", 30, "83b9c8fc8cbf2c6af2f343934b461732"},
    {"Nekketsu Kouha - Kunio Kun (J).nes", 60, "83b9c8fc8cbf2c6af2f343934b461732"},
    {"Puyo Puyo (J).nes", 30, "441003a5ae2ae49d035773c95d4feb76"},
    {"Puyo Puyo (J).nes", 60, "7e3000f32e9abe246f1a6fd9825bbdbe"},
    {"RXHPUS.nes", 30, "846cc844ca40ab46372852dae776a8af"},
    {"RXHPUS.nes", 60, "846cc844ca40ab46372852dae776a8af"},
    {"Rainbow Islands - The Story of Bubble Bobble 2 (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Rainbow Islands - The Story of Bubble Bobble 2 (J).nes", 60, "010ff406efb407b28c185bbdca3a4a11"},
    {"Rush'n Attack (U).nes", 30, "eb682b3dda40a0b1584062439d4d3058"},
    {"Rush'n Attack (U).nes", 60, "778e1ebd72bab9d86ad61e5c088af6c3"},
    {"Saiyuuki World (J).nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"Saiyuuki World (J).nes", 60, "84d48c37bf0285efb9f780b857a03adf"},
    {"Shanghai (J).nes", 30, "eec7dc494e33e01df58cb77919807b77"},
    {"Shanghai (J).nes", 60, "eec7dc494e33e01df58cb77919807b77"},
    {"Shanghai 2 (J).nes", 30, "3fa9569e92d6f7e6f212bfcafa2c30c8"},
    {"Shanghai 2 (J).nes", 60, "3fa9569e92d6f7e6f212bfcafa2c30c8"},
    {"Top Gun (JE).nes", 30, "477e6fb210dd0da41147ba7c0835fbf1"},
    {"Top Gun (JE).nes", 60, "477e6fb210dd0da41147ba7c0835fbf1"},
    {"rainbowisland.nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"rainbowisland.nes", 60, "06fb028b9ad832a7666ab4f7c0e2fb4e"},
    {"sidepoo1.nes", 30, "fe6d1cf605ea36d725a848feed7ebdc8"},
    {"sidepoo1.nes", 60, "659325fd9ba6c26eee8db5e6ba48b8b7"},
};

class Mapper2RegressionTest : public ::testing::TestWithParam<Mapper2TestEntry> {};

TEST_P(Mapper2RegressionTest, Frame) {
    const auto& param = GetParam();
    std::string rom_path = std::string(EAR6_SOURCE_DIR) + "/assets/nes/rom/mapper_2/" + param.filename;
    FILE* rom_file = std::fopen(rom_path.c_str(), "rb");
    if (!rom_file) {
        GTEST_SKIP() << "Missing test ROM: " << rom_path;
    }
    std::fclose(rom_file);

    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);

    int rc = ear6_load(ctx, rom_path.c_str());
    ASSERT_EQ(rc, 0) << "ear6_load failed for: " << rom_path;

    for (int i = 0; i < param.frame; i++) {
        ASSERT_EQ(ear6_step(ctx), 0);
    }

    const uint8_t* fb = ear6_get_framebuffer(ctx);
    ASSERT_NE(fb, nullptr);
    ASSERT_EQ(ear6_get_frame_width(ctx), 256);
    ASSERT_EQ(ear6_get_frame_height(ctx), 240);

    std::string hash = ppm_md5(fb, 256, 240);
    EXPECT_EQ(hash, param.expected_md5);

    ear6_destroy(ctx);
}

INSTANTIATE_TEST_SUITE_P(Mapper2Regression, Mapper2RegressionTest,
    ::testing::ValuesIn(kMapper2Tests));
