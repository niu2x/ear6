#include <ear6/ear6.h>
#include <ear6/nes.h>

#include "system.h"
#include "system_test.h"
#include "state_stream.h"
#include "nes/nes_system.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct Ear6 {
    Ear6SystemType system_type;
    std::unique_ptr<ear6::System> system;
    Ear6FrameCallback frame_cb = nullptr;
    void* frame_user_data = nullptr;
    Ear6AudioCallback audio_cb = nullptr;
    void* audio_user_data = nullptr;
    bool has_content = false;
    std::vector<uint8_t> content;
    std::string content_name_hint;
};

namespace {

constexpr uint8_t STATE_MAGIC[8] = {'E', 'A', 'R', '6', 'S', 'T', 'A', 'T'};
constexpr uint32_t STATE_FLAG_HAS_CONTENT = 1u << 0;
constexpr uint32_t STATE_KNOWN_FLAGS = STATE_FLAG_HAS_CONTENT;
constexpr size_t STATE_HEADER_SIZE = 64;
constexpr uint64_t MAX_STATE_NAME_HINT_SIZE = 4096;

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

std::string normalize_content_name_hint(const char* name_hint) {
    if (!name_hint) return {};
    std::string name(name_hint);
    size_t query = name.find_first_of("?#");
    if (query != std::string::npos) name.resize(query);
    size_t separator = name.find_last_of("/\\");
    if (separator != std::string::npos) name.erase(0, separator + 1);
    if (name.size() > MAX_STATE_NAME_HINT_SIZE) {
        name.erase(0, name.size() - MAX_STATE_NAME_HINT_SIZE);
    }
    return name;
}

int build_state(Ear6* ctx, std::vector<uint8_t>& state) {
    std::vector<uint8_t> payload;
    int result = ctx->system->save_state(payload);
    if (result != 0) return result;

    ear6::StateStream stream;
    uint8_t magic[sizeof(STATE_MAGIC)];
    std::memcpy(magic, STATE_MAGIC, sizeof(magic));
    stream.sync_bytes(magic, sizeof(magic));

    uint32_t version = EAR6_STATE_FORMAT_VERSION;
    uint64_t content_identity = ctx->system->get_content_identity();
    uint64_t content_size = ctx->content.size();
    uint64_t name_hint_size = ctx->content_name_hint.size();
    uint64_t payload_size = payload.size();
    uint32_t flags = ctx->has_content ? STATE_FLAG_HAS_CONTENT : 0;
    uint32_t reserved_32 = 0;
    uint64_t reserved_64 = 0;

    std::vector<uint8_t> body;
    body.reserve(ctx->content_name_hint.size() + ctx->content.size() + payload.size());
    body.insert(body.end(), ctx->content_name_hint.begin(), ctx->content_name_hint.end());
    body.insert(body.end(), ctx->content.begin(), ctx->content.end());
    body.insert(body.end(), payload.begin(), payload.end());
    uint32_t body_crc = crc32(body.data(), body.size());

    stream.sync(version);
    stream.sync(flags);
    stream.sync(content_identity);
    stream.sync(content_size);
    stream.sync(name_hint_size);
    stream.sync(payload_size);
    stream.sync(body_crc);
    stream.sync(reserved_32);
    stream.sync(reserved_64);
    if (!body.empty()) {
        stream.sync_bytes(body.data(), body.size());
    }
    state = stream.get_data();
    return 0;
}

int load_content(
    Ear6* ctx,
    const void* data,
    int size,
    const char* name_hint
) {
    if (size < 0 || (!data && size > 0)) return -1;

    std::vector<uint8_t> content;
    if (size > 0) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        content.assign(bytes, bytes + size);
    }
    std::string stored_name_hint = normalize_content_name_hint(name_hint);

    int result = ctx->system->load_from_memory(data, size, name_hint);
    if (result != 0) return result;

    ctx->content = std::move(content);
    ctx->content_name_hint = std::move(stored_name_hint);
    ctx->has_content = true;
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
        if (sz > std::numeric_limits<int>::max()) { std::fclose(f); return -3; }
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        std::rewind(f);
        if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
            std::fclose(f);
            return -3;
        }
        std::fclose(f);
        return load_content(ctx, buf.data(), static_cast<int>(sz), path);
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
        return load_content(ctx, data, size, name_hint);
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
        uint32_t flags = 0;
        uint64_t content_identity = 0;
        uint64_t content_size = 0;
        uint64_t name_hint_size = 0;
        uint64_t payload_size = 0;
        uint32_t body_crc = 0;
        uint32_t reserved_32 = 0;
        uint64_t reserved_64 = 0;
        stream.sync_bytes(magic, sizeof(magic));
        stream.sync(version);
        stream.sync(flags);
        stream.sync(content_identity);
        stream.sync(content_size);
        stream.sync(name_hint_size);
        stream.sync(payload_size);
        stream.sync(body_crc);
        stream.sync(reserved_32);
        stream.sync(reserved_64);

        if (stream.has_error()
            || std::memcmp(magic, STATE_MAGIC, sizeof(magic)) != 0
            || version != EAR6_STATE_FORMAT_VERSION
            || (flags & ~STATE_KNOWN_FLAGS) != 0
            || ((flags & STATE_FLAG_HAS_CONTENT) == 0
                && (content_size != 0 || name_hint_size != 0))
            || reserved_32 != 0
            || reserved_64 != 0
            || name_hint_size > MAX_STATE_NAME_HINT_SIZE
            || content_size > static_cast<uint64_t>(std::numeric_limits<int>::max())
            || name_hint_size > stream.get_remaining()) {
            return -4;
        }

        size_t remaining = stream.get_remaining();
        remaining -= static_cast<size_t>(name_hint_size);
        if (content_size > remaining) return -4;
        remaining -= static_cast<size_t>(content_size);
        if (payload_size != remaining) return -4;

        const auto* body = static_cast<const uint8_t*>(data) + STATE_HEADER_SIZE;
        if (body_crc != crc32(body, size - STATE_HEADER_SIZE)) return -4;

        std::string name_hint(
            reinterpret_cast<const char*>(body),
            static_cast<size_t>(name_hint_size)
        );
        if (name_hint.find('\0') != std::string::npos) return -4;
        const uint8_t* content = body + name_hint_size;
        const uint8_t* payload = content + content_size;

        std::vector<uint8_t> stored_content;
        if ((flags & STATE_FLAG_HAS_CONTENT) != 0 && content_size > 0) {
            stored_content.assign(content, content + content_size);
        }
        auto candidate = create_system(ctx->system_type);
        if ((flags & STATE_FLAG_HAS_CONTENT) != 0) {
            const char* hint = name_hint.empty() ? nullptr : name_hint.c_str();
            int result = candidate->load_from_memory(
                content,
                static_cast<int>(content_size),
                hint
            );
            if (result != 0) return result;
        }
        if (candidate->get_content_identity() != content_identity) return -4;
        int result = candidate->load_state(payload, static_cast<size_t>(payload_size));
        if (result != 0) return result;

        ctx->system = std::move(candidate);
        ctx->has_content = (flags & STATE_FLAG_HAS_CONTENT) != 0;
        ctx->content = std::move(stored_content);
        ctx->content_name_hint = std::move(name_hint);
        return 0;
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
