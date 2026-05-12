#include <ear6/ear6.h>

#include "system.h"
#include "system_test.h"

#include <memory>
#include <stdexcept>

struct Ear6 {
    Ear6SystemType system_type;
    std::unique_ptr<ear6::System> system;
    Ear6FrameCallback frame_cb = nullptr;
    void* frame_user_data = nullptr;
    Ear6AudioCallback audio_cb = nullptr;
    void* audio_user_data = nullptr;
};

static ear6::System* create_system(Ear6SystemType type) {
    switch (type) {
        case EAR6_SYSTEM_TEST:
            return new ear6::TestSystem();
        case EAR6_SYSTEM_NES:
        case EAR6_SYSTEM_FLASH:
            throw std::runtime_error("system not implemented");
    }
    throw std::runtime_error("unknown system type");
}

extern "C" Ear6* ear6_create(Ear6SystemType system) {
    try {
        auto* ctx = new Ear6();
        ctx->system_type = system;
        ctx->system.reset(create_system(system));
        return ctx;
    } catch (...) {
        return nullptr;
    }
}

extern "C" void ear6_destroy(Ear6* ctx) {
    delete ctx;
}

extern "C" int ear6_load(Ear6* ctx, const void* data, int size) {
    if (!ctx || !ctx->system) return -1;
    try {
        return ctx->system->load(data, size);
    } catch (...) {
        return -2;
    }
}

extern "C" void ear6_set_frame_callback(Ear6* ctx, Ear6FrameCallback cb, void* user_data) {
    if (!ctx) return;
    ctx->frame_cb = cb;
    ctx->frame_user_data = user_data;
}

extern "C" void ear6_set_audio_callback(Ear6* ctx, Ear6AudioCallback cb, void* user_data) {
    if (!ctx) return;
    ctx->audio_cb = cb;
    ctx->audio_user_data = user_data;
}

extern "C" int ear6_step(Ear6* ctx) {
    if (!ctx || !ctx->system) return -1;
    try {
        int result = ctx->system->step();
        if (result == 0) {
            if (ctx->frame_cb) {
                ctx->frame_cb(
                    ctx->system->framebuffer(),
                    ctx->system->frame_width(),
                    ctx->system->frame_height(),
                    ctx->frame_user_data
                );
            }
            if (ctx->audio_cb && ctx->system->audio_num_samples() > 0) {
                ctx->audio_cb(
                    ctx->system->audiobuffer(),
                    ctx->system->audio_num_samples(),
                    ctx->audio_user_data
                );
            }
        }
        return result;
    } catch (...) {
        return -2;
    }
}

extern "C" int ear6_test(void) {
    return 42;
}

extern "C" const uint8_t* ear6_get_framebuffer(Ear6* ctx) {
    if (!ctx || !ctx->system) return nullptr;
    return ctx->system->framebuffer();
}

extern "C" int ear6_get_frame_width(Ear6* ctx) {
    if (!ctx || !ctx->system) return 0;
    return ctx->system->frame_width();
}

extern "C" int ear6_get_frame_height(Ear6* ctx) {
    if (!ctx || !ctx->system) return 0;
    return ctx->system->frame_height();
}

extern "C" const int16_t* ear6_get_audiobuffer(Ear6* ctx) {
    if (!ctx || !ctx->system) return nullptr;
    return ctx->system->audiobuffer();
}

extern "C" int ear6_get_audio_num_samples(Ear6* ctx) {
    if (!ctx || !ctx->system) return 0;
    return ctx->system->audio_num_samples();
}
