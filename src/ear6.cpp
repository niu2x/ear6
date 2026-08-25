#include <ear6/ear6.h>
#include <ear6/nes.h>

#include "system.h"
#include "system_test.h"
#include "state_stream.h"
#include "nes/nes_system.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

struct Ear6 {
    Ear6SystemType system_type;
    std::unique_ptr<ear6::System> system;
    Ear6FrameCallback frame_cb = nullptr;
    void* frame_user_data = nullptr;
    Ear6AudioCallback audio_cb = nullptr;
    void* audio_user_data = nullptr;
};

namespace {

constexpr uint8_t STATE_MAGIC[8] = {'E', 'A', 'R', '6', 'S', 'T', 'A', 'T'};
constexpr uint32_t STATE_FORMAT_VERSION = 1;

uint32_t crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

int build_state(Ear6* ctx, std::vector<uint8_t>& state) {
    std::vector<uint8_t> payload;
    int result = ctx->system->save_state(payload);
    if (result != 0) return result;

    ear6::StateStream stream;
    uint8_t magic[sizeof(STATE_MAGIC)];
    std::memcpy(magic, STATE_MAGIC, sizeof(magic));
    stream.sync_bytes(magic, sizeof(magic));

    uint32_t version = STATE_FORMAT_VERSION;
    uint32_t system_type = static_cast<uint32_t>(ctx->system_type);
    uint64_t content_identity = ctx->system->get_content_identity();
    uint64_t payload_size = payload.size();
    uint32_t payload_crc = crc32(payload.data(), payload.size());
    uint32_t reserved = 0;
    stream.sync(version);
    stream.sync(system_type);
    stream.sync(content_identity);
    stream.sync(payload_size);
    stream.sync(payload_crc);
    stream.sync(reserved);
    if (!payload.empty()) {
        stream.sync_bytes(payload.data(), payload.size());
    }
    state = stream.get_data();
    return 0;
}

} // namespace

static std::unique_ptr<ear6::System> create_system(Ear6SystemType type) {
    switch (type) {
        case EAR6_SYSTEM_TEST:
            return std::make_unique<ear6::TestSystem>();
        case EAR6_SYSTEM_NES:
            return std::make_unique<ear6::NesSystem>();
        case EAR6_SYSTEM_FLASH:
            throw std::runtime_error("system not implemented");
    }
    throw std::runtime_error("unknown system type");
}

extern "C" Ear6* ear6_create(Ear6SystemType system) {
    try {
        auto ctx = std::make_unique<Ear6>();
        ctx->system_type = system;
        ctx->system = create_system(system);
        return ctx.release();
    } catch (...) {
        return nullptr;
    }
}

extern "C" void ear6_destroy(Ear6* ctx) {
    delete ctx;
}

extern "C" int ear6_load_from_file(Ear6* ctx, const char* path) {
    if (!ctx || !ctx->system || !path) return -1;
    try {
        FILE* f = std::fopen(path, "rb");
        if (!f) return -3;
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        if (sz < 0) { std::fclose(f); return -3; }
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        std::rewind(f);
        if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
            std::fclose(f);
            return -3;
        }
        std::fclose(f);
        return ctx->system->load_from_memory(buf.data(), static_cast<int>(sz), path);
    } catch (...) {
        return -2;
    }
}

extern "C" int ear6_load_from_memory(
    Ear6* ctx,
    const void* data,
    int size,
    const char* name_hint
) {
    if (!ctx || !ctx->system) return -1;
    try {
        return ctx->system->load_from_memory(data, size, name_hint);
    } catch (...) {
        return -2;
    }
}

extern "C" int ear6_save_state_to_memory(
    Ear6* ctx,
    void* buffer,
    size_t capacity,
    size_t* state_size
) {
    if (!ctx || !ctx->system || !state_size) return -1;
    try {
        std::vector<uint8_t> state;
        int result = build_state(ctx, state);
        if (result != 0) return result;

        *state_size = state.size();
        if (!buffer) return 0;
        if (capacity < state.size()) return -4;
        std::memcpy(buffer, state.data(), state.size());
        return 0;
    } catch (...) {
        return -2;
    }
}

extern "C" int ear6_load_state_from_memory(Ear6* ctx, const void* data, size_t size) {
    if (!ctx || !ctx->system || !data) return -1;
    try {
        ear6::StateStream stream(data, size);
        uint8_t magic[sizeof(STATE_MAGIC)] = {};
        uint32_t version = 0;
        uint32_t system_type = 0;
        uint64_t content_identity = 0;
        uint64_t payload_size = 0;
        uint32_t payload_crc = 0;
        uint32_t reserved = 0;
        stream.sync_bytes(magic, sizeof(magic));
        stream.sync(version);
        stream.sync(system_type);
        stream.sync(content_identity);
        stream.sync(payload_size);
        stream.sync(payload_crc);
        stream.sync(reserved);

        if (stream.has_error()
            || std::memcmp(magic, STATE_MAGIC, sizeof(magic)) != 0
            || version != STATE_FORMAT_VERSION
            || system_type != static_cast<uint32_t>(ctx->system_type)
            || content_identity != ctx->system->get_content_identity()
            || reserved != 0
            || payload_size != stream.get_remaining()
            || payload_crc != crc32(
                static_cast<const uint8_t*>(data) + (size - stream.get_remaining()),
                stream.get_remaining()
            )) {
            return -4;
        }

        const auto* payload = static_cast<const uint8_t*>(data) + (size - stream.get_remaining());
        std::vector<uint8_t> backup;
        if (ctx->system->save_state(backup) != 0) return -4;

        int result = 0;
        try {
            result = ctx->system->load_state(payload, stream.get_remaining());
        } catch (...) {
            try {
                ctx->system->load_state(backup.data(), backup.size());
            } catch (...) {
            }
            return -2;
        }
        if (result != 0) {
            ctx->system->load_state(backup.data(), backup.size());
        }
        return result;
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
                    ctx->system->get_framebuffer(),
                    ctx->system->get_frame_width(),
                    ctx->system->get_frame_height(),
                    ctx->frame_user_data
                );
            }
            if (ctx->audio_cb && ctx->system->get_audio_num_samples() > 0) {
                ctx->audio_cb(
                    ctx->system->get_audiobuffer(),
                    ctx->system->get_audio_num_samples(),
                    ctx->audio_user_data
                );
                ctx->system->consume_audio();
            }
        }
        return result;
    } catch (...) {
        return -2;
    }
}

extern "C" int ear6_nes_set_palette(Ear6* ctx, const uint32_t palette[64]) {
    if (!ctx || !ctx->system || ctx->system_type != EAR6_SYSTEM_NES) return -1;
    try {
        static_cast<ear6::NesSystem*>(ctx->system.get())->set_palette(palette);
        return 0;
    } catch (...) {
        return -2;
    }
}

extern "C" int ear6_nes_set_button_state(Ear6* ctx, Ear6NesButton button, int pressed) {
    if (!ctx || !ctx->system || ctx->system_type != EAR6_SYSTEM_NES) return -1;
    try {
        auto* nes = static_cast<ear6::NesSystem*>(ctx->system.get());
        nes->get_console()->set_button_state(0, static_cast<int>(button), pressed != 0);
        return 0;
    } catch (...) {
        return -2;
    }
}

extern "C" void ear6_nes_clear_input(Ear6* ctx) {
    if (!ctx || !ctx->system || ctx->system_type != EAR6_SYSTEM_NES) return;
    try {
        auto* nes = static_cast<ear6::NesSystem*>(ctx->system.get());
        nes->get_console()->clear_input();
    } catch (...) {
    }
}

extern "C" int ear6_test(void) {
    return 42;
}

extern "C" const uint8_t* ear6_get_framebuffer(Ear6* ctx) {
    if (!ctx || !ctx->system) return nullptr;
    return ctx->system->get_framebuffer();
}

extern "C" int ear6_get_frame_width(Ear6* ctx) {
    if (!ctx || !ctx->system) return 0;
    return ctx->system->get_frame_width();
}

extern "C" int ear6_get_frame_height(Ear6* ctx) {
    if (!ctx || !ctx->system) return 0;
    return ctx->system->get_frame_height();
}

extern "C" const int16_t* ear6_get_audiobuffer(Ear6* ctx) {
    if (!ctx || !ctx->system) return nullptr;
    return ctx->system->get_audiobuffer();
}

extern "C" int ear6_get_audio_num_samples(Ear6* ctx) {
    if (!ctx || !ctx->system) return 0;
    return ctx->system->get_audio_num_samples();
}

extern "C" void ear6_consume_audio(Ear6* ctx) {
    if (!ctx || !ctx->system) return;
    ctx->system->consume_audio();
}
