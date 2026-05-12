#include <ear6/ear6.h>
#include <cstdio>

static void on_frame(const void* data, int width, int height, void* user_data) {
    (void)user_data;
    std::printf("frame: %dx%d, ptr=%p\n", width, height, data);
}

int main() {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    if (!ctx) {
        std::printf("ear6_create failed\n");
        return 1;
    }

    ear6_set_frame_callback(ctx, on_frame, nullptr);
    ear6_load(ctx, nullptr, 0);

    for (int i = 0; i < 3; ++i) {
        ear6_step(ctx);
    }

    ear6_destroy(ctx);
    return 0;
}
