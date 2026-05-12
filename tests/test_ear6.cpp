#include <ear6/ear6.h>

#include <gtest/gtest.h>

TEST(Ear6Create, TestSystemSuccess) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ear6_destroy(ctx);
}

TEST(Ear6Create, UnimplementedSystemReturnsNull) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    EXPECT_EQ(ctx, nullptr);

    ctx = ear6_create(EAR6_SYSTEM_FLASH);
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
