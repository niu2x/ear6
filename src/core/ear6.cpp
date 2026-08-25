#include <ear6/ear6.h>
#include <ear6/nes.h>

#include "system.h"
#include "../systems/test/test_system.h"
#include "state/state_container.upb.h"
#include "../systems/nes/nes_system.h"

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
constexpr size_t STATE_PREAMBLE_SIZE = 32;
constexpr uint64_t MAX_STATE_NAME_HINT_SIZE = 4096;
constexpr uint32_t STATE_PREVIEW_VERSION = 1;

struct ParsedState {
    uint32_t container_wire_version = 0;
    Ear6SystemType system_type = EAR6_SYSTEM_TEST;
    uint64_t content_identity = 0;
    uint64_t content_size = 0;
    const char* name_hint = nullptr;
    size_t name_hint_size = 0;
    const uint8_t* content = nullptr;
    const uint8_t* system_state = nullptr;
    size_t system_state_size = 0;
    Ear6StatePreviewFormat preview_format = EAR6_STATE_PREVIEW_NONE;
    const uint8_t* preview_data = nullptr;
    size_t preview_size = 0;
    int preview_width = 0;
    int preview_height = 0;
};

void append_u32(std::vector<uint8_t>& data, uint32_t value) {
    for (size_t i = 0; i < sizeof(value); ++i) {
        data.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
}

void append_u64(std::vector<uint8_t>& data, uint64_t value) {
    for (size_t i = 0; i < sizeof(value); ++i) {
        data.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
}

uint32_t read_u32(const uint8_t* data) {
    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(value); ++i) {
        value |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    return value;
}

uint64_t read_u64(const uint8_t* data) {
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(value); ++i) {
        value |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    return value;
}

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

uint64_t content_identity(const uint8_t* data, size_t size) {
    uint64_t identity = 1469598103934665603ULL;
    for (size_t i = 0; i < size; ++i) {
        identity ^= data[i];
        identity *= 1099511628211ULL;
    }
    return identity;
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

bool checked_rgba_size(uint32_t width, uint32_t height, size_t* size) {
    if (!size || width == 0 || height == 0
        || width > static_cast<uint32_t>(std::numeric_limits<int>::max())
        || height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    if (width > std::numeric_limits<size_t>::max() / height / 4) return false;
    *size = static_cast<size_t>(width) * height * 4;
    return true;
}

void parse_state_preview(const ear6_state_Preview* preview, ParsedState& parsed) {
    if (!preview
        || !ear6_state_Preview_has_version(preview)
        || !ear6_state_Preview_has_format(preview)
        || !ear6_state_Preview_has_width(preview)
        || !ear6_state_Preview_has_height(preview)
        || !ear6_state_Preview_has_image_data(preview)) {
        return;
    }

    uint32_t preview_version = ear6_state_Preview_version(preview);
    int32_t format = ear6_state_Preview_format(preview);
    uint32_t width = ear6_state_Preview_width(preview);
    uint32_t height = ear6_state_Preview_height(preview);
    upb_StringView image_data = ear6_state_Preview_image_data(preview);
    size_t rgba_size = 0;
    if (preview_version != STATE_PREVIEW_VERSION
        || format != ear6_state_Preview_FORMAT_RGBA8888
        || !checked_rgba_size(width, height, &rgba_size)
        || rgba_size != image_data.size) {
        return;
    }

    parsed.preview_format = EAR6_STATE_PREVIEW_RGBA8888;
    parsed.preview_data = reinterpret_cast<const uint8_t*>(image_data.data);
    parsed.preview_size = rgba_size;
    parsed.preview_width = static_cast<int>(width);
    parsed.preview_height = static_cast<int>(height);
}

int parse_state(const void* data, size_t size, ParsedState& parsed) {
    if (!data) return -1;
    if (size < STATE_PREAMBLE_SIZE) return -4;

    const auto* bytes = static_cast<const uint8_t*>(data);
    if (std::memcmp(bytes, STATE_MAGIC, sizeof(STATE_MAGIC)) != 0) return -4;

    uint32_t wire_version = read_u32(bytes + 8);
    uint32_t preamble_size = read_u32(bytes + 12);
    uint64_t body_size = read_u64(bytes + 16);
    uint32_t body_crc = read_u32(bytes + 24);
    uint32_t reserved = read_u32(bytes + 28);
    if (wire_version != EAR6_STATE_CONTAINER_WIRE_VERSION
        || preamble_size < STATE_PREAMBLE_SIZE
        || preamble_size > size
        || body_size != static_cast<uint64_t>(size - preamble_size)
        || reserved != 0) {
        return -4;
    }

    const uint8_t* body = bytes + preamble_size;
    const size_t body_size_value = size - preamble_size;
    if (body_crc != crc32(body, body_size_value)) return -4;

    upb_Arena* arena = upb_Arena_New();
    if (!arena) return -2;
    const int options = kUpb_DecodeOption_AliasString
        | kUpb_DecodeOption_CheckRequired;
    ear6_state_StateContainer* container = ear6_state_StateContainer_parse_ex(
        reinterpret_cast<const char*>(body), body_size_value, nullptr, options, arena);
    if (!container) {
        upb_Arena_Free(arena);
        return -4;
    }

    upb_StringView content = ear6_state_StateContainer_content(container);
    upb_StringView system_state = ear6_state_StateContainer_system_state(container);
    if (content.size > static_cast<size_t>(std::numeric_limits<int>::max())) {
        upb_Arena_Free(arena);
        return -4;
    }

    parsed.system_type = static_cast<Ear6SystemType>(
        ear6_state_StateContainer_system_type(container));
    parsed.content_identity = ear6_state_StateContainer_content_identity(container);
    parsed.content = reinterpret_cast<const uint8_t*>(content.data);
    parsed.content_size = content.size;
    parsed.system_state = reinterpret_cast<const uint8_t*>(system_state.data);
    parsed.system_state_size = system_state.size;
    if (ear6_state_StateContainer_has_content_name(container)) {
        upb_StringView name = ear6_state_StateContainer_content_name(container);
        if (name.size <= MAX_STATE_NAME_HINT_SIZE
            && std::memchr(name.data, '\0', name.size) == nullptr) {
            parsed.name_hint = name.data;
            parsed.name_hint_size = name.size;
        }
    }
    if (ear6_state_StateContainer_has_preview(container)) {
        parse_state_preview(ear6_state_StateContainer_preview(container), parsed);
    }
    upb_Arena_Free(arena);

    if (parsed.content_identity != content_identity(
            parsed.content, static_cast<size_t>(parsed.content_size))) {
        return -4;
    }
    parsed.container_wire_version = wire_version;
    return 0;
}

int build_state(Ear6* ctx, std::vector<uint8_t>& state) {
    std::vector<uint8_t> system_state;
    int result = ctx->system->save_state(system_state);
    if (result != 0) return result;
    upb_Arena* arena = upb_Arena_New();
    if (!arena) return -2;
    ear6_state_StateContainer* container = ear6_state_StateContainer_new(arena);
    if (!container) {
        upb_Arena_Free(arena);
        return -2;
    }

    ear6_state_StateContainer_set_system_type(
        container, static_cast<uint32_t>(ctx->system_type));
    ear6_state_StateContainer_set_content_identity(
        container, content_identity(ctx->content.data(), ctx->content.size()));
    if (!ctx->content_name_hint.empty()) {
        ear6_state_StateContainer_set_content_name(container, upb_StringView_FromDataAndSize(
            ctx->content_name_hint.data(), ctx->content_name_hint.size()));
    }
    ear6_state_StateContainer_set_content(container, upb_StringView_FromDataAndSize(
        reinterpret_cast<const char*>(ctx->content.data()), ctx->content.size()));
    ear6_state_StateContainer_set_system_state(container, upb_StringView_FromDataAndSize(
        reinterpret_cast<const char*>(system_state.data()), system_state.size()));

    const uint8_t* framebuffer = ctx->system->get_framebuffer();
    int width = ctx->system->get_frame_width();
    int height = ctx->system->get_frame_height();
    size_t preview_size = 0;
    if (framebuffer && width > 0 && height > 0
        && checked_rgba_size(
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            &preview_size)) {
        ear6_state_Preview* preview = ear6_state_StateContainer_mutable_preview(
            container, arena);
        if (!preview) {
            upb_Arena_Free(arena);
            return -2;
        }
        ear6_state_Preview_set_version(preview, STATE_PREVIEW_VERSION);
        ear6_state_Preview_set_format(preview, ear6_state_Preview_FORMAT_RGBA8888);
        ear6_state_Preview_set_width(preview, static_cast<uint32_t>(width));
        ear6_state_Preview_set_height(preview, static_cast<uint32_t>(height));
        ear6_state_Preview_set_image_data(preview, upb_StringView_FromDataAndSize(
            reinterpret_cast<const char*>(framebuffer), preview_size));
    }

    char* body_data = nullptr;
    size_t body_size = 0;
    upb_EncodeStatus encode_status = upb_Encode(
        UPB_UPCAST(container),
        &ear6__state__StateContainer_msg_init,
        kUpb_EncodeOption_Deterministic | kUpb_EncodeOption_CheckRequired,
        arena,
        &body_data,
        &body_size);
    if (encode_status != kUpb_EncodeStatus_Ok) {
        upb_Arena_Free(arena);
        return -2;
    }

    state.clear();
    state.reserve(STATE_PREAMBLE_SIZE + body_size);
    state.insert(state.end(), std::begin(STATE_MAGIC), std::end(STATE_MAGIC));
    append_u32(state, EAR6_STATE_CONTAINER_WIRE_VERSION);
    append_u32(state, STATE_PREAMBLE_SIZE);
    append_u64(state, body_size);
    append_u32(state, crc32(reinterpret_cast<const uint8_t*>(body_data), body_size));
    append_u32(state, 0);
    if (body_size > 0) {
        const auto* body_bytes = reinterpret_cast<const uint8_t*>(body_data);
        state.insert(state.end(), body_bytes, body_bytes + body_size);
    }
    upb_Arena_Free(arena);
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
        ParsedState parsed;
        int parse_result = parse_state(data, size, parsed);
        if (parse_result != 0) return parse_result;
        if (parsed.system_type != ctx->system_type) return -4;

        std::string name_hint(parsed.name_hint, parsed.name_hint_size);

        std::vector<uint8_t> stored_content;
        if (parsed.content_size > 0) {
            stored_content.assign(parsed.content, parsed.content + parsed.content_size);
        }
        auto candidate = create_system(ctx->system_type);
        const char* hint = name_hint.empty() ? nullptr : name_hint.c_str();
        int result = candidate->load_from_memory(
            parsed.content,
            static_cast<int>(parsed.content_size),
            hint
        );
        if (result != 0) return result;
        result = candidate->load_state(parsed.system_state, parsed.system_state_size);
        if (result != 0) return result;

        ctx->system = std::move(candidate);
        ctx->has_content = true;
        ctx->content = std::move(stored_content);
        ctx->content_name_hint = std::move(name_hint);
        return 0;
    } catch (...) {
        return -2;
    }
}

extern "C" int ear6_get_state_info(const void* data, size_t size, Ear6StateInfo* info) {
    if (!info) return -1;
    std::memset(info, 0, sizeof(*info));
    try {
        ParsedState parsed;
        int result = parse_state(data, size, parsed);
        if (result != 0) return result;

        info->container_wire_version = parsed.container_wire_version;
        info->system_type = parsed.system_type;
        info->content_identity = parsed.content_identity;
        info->content_size = parsed.content_size;
        info->content_name_hint = parsed.name_hint_size > 0 ? parsed.name_hint : nullptr;
        info->content_name_hint_size = parsed.name_hint_size;
        info->preview_format = parsed.preview_format;
        info->preview_data = parsed.preview_data;
        info->preview_size = parsed.preview_size;
        info->preview_width = parsed.preview_width;
        info->preview_height = parsed.preview_height;
        return 0;
    } catch (...) {
        std::memset(info, 0, sizeof(*info));
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
