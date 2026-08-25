#include <ear6/ear6.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <gtest/gtest.h>

TEST(Ear6State, TestSystemRoundTrip) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);

    for (int i = 0; i < 5; ++i) ASSERT_EQ(ear6_step(ctx), 0);

    size_t state_size = 0;
    ASSERT_EQ(ear6_save_state_to_memory(ctx, nullptr, 0, &state_size), 0);
    ASSERT_GT(state_size, 0u);
    std::vector<uint8_t> state(state_size);
    ASSERT_EQ(ear6_save_state_to_memory(ctx, state.data(), state.size(), &state_size), 0);

    for (int i = 0; i < 3; ++i) ASSERT_EQ(ear6_step(ctx), 0);
    const uint8_t* expected_data = ear6_get_framebuffer(ctx);
    ASSERT_NE(expected_data, nullptr);
    std::vector<uint8_t> expected(expected_data, expected_data + 256 * 240 * 4);

    ASSERT_EQ(ear6_load_state_from_memory(ctx, state.data(), state.size()), 0);
    for (int i = 0; i < 3; ++i) ASSERT_EQ(ear6_step(ctx), 0);
    const uint8_t* actual = ear6_get_framebuffer(ctx);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(std::memcmp(expected.data(), actual, expected.size()), 0);

    ear6_destroy(ctx);
}

TEST(Ear6State, RejectsSmallOutputBuffer) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);

    uint8_t buffer[1] = {};
    size_t required = 0;
    EXPECT_NE(ear6_save_state_to_memory(ctx, buffer, sizeof(buffer), &required), 0);
    EXPECT_GT(required, sizeof(buffer));

    ear6_destroy(ctx);
}

TEST(Ear6State, RejectsInvalidStateWithoutMutation) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(ear6_step(ctx), 0);

    const uint8_t* before_data = ear6_get_framebuffer(ctx);
    std::vector<uint8_t> before(before_data, before_data + 256 * 240 * 4);
    const uint8_t invalid[] = {'N', 'O', 'T', 'S', 'T', 'A', 'T', 'E'};
    EXPECT_NE(ear6_load_state_from_memory(ctx, invalid, sizeof(invalid)), 0);
    EXPECT_EQ(std::memcmp(before.data(), ear6_get_framebuffer(ctx), before.size()), 0);

    ear6_destroy(ctx);
}

TEST(Ear6State, RejectsOlderFormatVersionWithoutMutation) {
    static_assert(
        EAR6_STATE_CONTAINER_WIRE_VERSION > 0,
        "state container wire version must be positive");

    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(ear6_step(ctx), 0);

    size_t state_size = 0;
    ASSERT_EQ(ear6_save_state_to_memory(ctx, nullptr, 0, &state_size), 0);
    std::vector<uint8_t> state(state_size);
    ASSERT_EQ(ear6_save_state_to_memory(ctx, state.data(), state.size(), &state_size), 0);

    constexpr size_t VERSION_OFFSET = 8;
    uint32_t old_version = EAR6_STATE_CONTAINER_WIRE_VERSION - 1;
    for (size_t i = 0; i < sizeof(old_version); ++i) {
        state[VERSION_OFFSET + i] = static_cast<uint8_t>(old_version >> (i * 8));
    }

    const uint8_t* before_data = ear6_get_framebuffer(ctx);
    std::vector<uint8_t> before(before_data, before_data + 256 * 240 * 4);
    EXPECT_NE(ear6_load_state_from_memory(ctx, state.data(), state.size()), 0);
    EXPECT_EQ(std::memcmp(before.data(), ear6_get_framebuffer(ctx), before.size()), 0);

    ear6_destroy(ctx);
}

TEST(Ear6State, NullArgumentsAreRejected) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    size_t size = 0;

    EXPECT_NE(ear6_save_state_to_memory(nullptr, nullptr, 0, &size), 0);
    EXPECT_NE(ear6_save_state_to_memory(ctx, nullptr, 0, nullptr), 0);
    EXPECT_NE(ear6_load_state_from_memory(nullptr, &size, sizeof(size)), 0);
    EXPECT_NE(ear6_load_state_from_memory(ctx, nullptr, 0), 0);
    Ear6StateInfo info = {};
    EXPECT_NE(ear6_get_state_info(nullptr, 0, &info), 0);
    EXPECT_NE(ear6_get_state_info(&size, sizeof(size), nullptr), 0);

    ear6_destroy(ctx);
}

static std::vector<uint8_t> save_state(Ear6* ctx) {
    size_t size = 0;
    if (ear6_save_state_to_memory(ctx, nullptr, 0, &size) != 0) return {};
    std::vector<uint8_t> state(size);
    if (ear6_save_state_to_memory(ctx, state.data(), state.size(), &size) != 0) return {};
    state.resize(size);
    return state;
}

static uint32_t state_crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static constexpr size_t STATE_PREAMBLE_SIZE = 32;
static constexpr size_t STATE_BODY_SIZE_OFFSET = 16;
static constexpr size_t STATE_BODY_CRC_OFFSET = 24;
static constexpr uint32_t STATE_FIELD_SYSTEM_TYPE = 1;
static constexpr uint32_t STATE_FIELD_CONTENT_IDENTITY = 2;
static constexpr uint32_t STATE_FIELD_CONTENT_NAME = 3;
static constexpr uint32_t STATE_FIELD_CONTENT = 4;
static constexpr uint32_t STATE_FIELD_SYSTEM_STATE = 5;
static constexpr uint32_t STATE_FIELD_PREVIEW = 6;

struct StateFieldRange {
    bool found = false;
    size_t field_offset = 0;
    size_t data_offset = 0;
    size_t size = 0;
    size_t end_offset = 0;
    uint32_t wire_type = 0;
};

static uint32_t read_u32_le(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + sizeof(uint32_t) > data.size()) return 0;
    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(value); ++i) {
        value |= static_cast<uint32_t>(data[offset + i]) << (i * 8);
    }
    return value;
}

static void write_u32_le(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    ASSERT_LE(offset + sizeof(value), data.size());
    for (size_t i = 0; i < sizeof(value); ++i) {
        data[offset + i] = static_cast<uint8_t>(value >> (i * 8));
    }
}

static void write_u64_le(std::vector<uint8_t>& data, size_t offset, uint64_t value) {
    ASSERT_LE(offset + sizeof(value), data.size());
    for (size_t i = 0; i < sizeof(value); ++i) {
        data[offset + i] = static_cast<uint8_t>(value >> (i * 8));
    }
}

static bool read_varint(
    const std::vector<uint8_t>& data,
    size_t offset,
    size_t limit,
    uint64_t* value,
    size_t* next
) {
    uint64_t result = 0;
    for (unsigned shift = 0; shift < 64 && offset < limit; shift += 7) {
        uint8_t byte = data[offset++];
        result |= static_cast<uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            *value = result;
            *next = offset;
            return true;
        }
    }
    return false;
}

static void append_varint(std::vector<uint8_t>& data, uint64_t value) {
    do {
        uint8_t byte = static_cast<uint8_t>(value & 0x7f);
        value >>= 7;
        data.push_back(value == 0 ? byte : static_cast<uint8_t>(byte | 0x80));
    } while (value != 0);
}

static StateFieldRange find_state_field(
    const std::vector<uint8_t>& state,
    uint32_t wanted_id,
    size_t body_offset = STATE_PREAMBLE_SIZE,
    size_t body_size = 0,
    size_t occurrence = 0
) {
    StateFieldRange result;
    if (body_offset > state.size()) return result;
    size_t limit = body_size == 0 ? state.size() : body_offset + body_size;
    if (limit > state.size()) return result;

    size_t offset = body_offset;
    while (offset < limit) {
        size_t field_offset = offset;
        uint64_t tag = 0;
        if (!read_varint(state, offset, limit, &tag, &offset) || tag == 0) return {};
        uint32_t field_id = static_cast<uint32_t>(tag >> 3);
        uint32_t wire_type = static_cast<uint32_t>(tag & 7);
        size_t data_offset = offset;
        size_t field_size = 0;
        if (wire_type == 0) {
            uint64_t ignored = 0;
            if (!read_varint(state, offset, limit, &ignored, &offset)) return {};
            field_size = offset - data_offset;
        } else if (wire_type == 1) {
            if (limit - offset < 8) return {};
            field_size = 8;
            offset += field_size;
        } else if (wire_type == 2) {
            uint64_t length = 0;
            if (!read_varint(state, offset, limit, &length, &offset)
                || length > limit - offset) {
                return {};
            }
            data_offset = offset;
            field_size = static_cast<size_t>(length);
            offset += field_size;
        } else if (wire_type == 5) {
            if (limit - offset < 4) return {};
            field_size = 4;
            offset += field_size;
        } else {
            return {};
        }
        if (field_id == wanted_id) {
            if (occurrence-- == 0) {
                result.found = true;
                result.field_offset = field_offset;
                result.data_offset = data_offset;
                result.size = field_size;
                result.end_offset = offset;
                result.wire_type = wire_type;
                return result;
            }
        }
    }
    return result;
}

static void refresh_state_container(std::vector<uint8_t>& state) {
    ASSERT_GE(state.size(), STATE_PREAMBLE_SIZE);
    size_t preamble_size = read_u32_le(state, 12);
    ASSERT_GE(preamble_size, STATE_PREAMBLE_SIZE);
    ASSERT_LE(preamble_size, state.size());
    write_u64_le(state, STATE_BODY_SIZE_OFFSET, state.size() - preamble_size);
    write_u32_le(state, STATE_BODY_CRC_OFFSET, state_crc32(
        state.data() + preamble_size, state.size() - preamble_size));
}

static void append_test_length_delimited_field(
    std::vector<uint8_t>& state,
    uint32_t field_id,
    const std::vector<uint8_t>& data
) {
    append_varint(state, (static_cast<uint64_t>(field_id) << 3) | 2);
    append_varint(state, data.size());
    state.insert(state.end(), data.begin(), data.end());
    refresh_state_container(state);
}

static void append_test_varint_field(
    std::vector<uint8_t>& state,
    uint32_t field_id,
    uint64_t value
) {
    append_varint(state, static_cast<uint64_t>(field_id) << 3);
    append_varint(state, value);
    refresh_state_container(state);
}

static void remove_state_field(std::vector<uint8_t>& state, uint32_t field_id) {
    StateFieldRange field = find_state_field(state, field_id);
    ASSERT_TRUE(field.found);
    state.erase(
        state.begin() + static_cast<std::ptrdiff_t>(field.field_offset),
        state.begin() + static_cast<std::ptrdiff_t>(field.end_offset));
    refresh_state_container(state);
}

static bool reverse_state_fields(std::vector<uint8_t>& state) {
    if (state.size() < STATE_PREAMBLE_SIZE) return false;
    size_t preamble_size = read_u32_le(state, 12);
    if (preamble_size < STATE_PREAMBLE_SIZE || preamble_size > state.size()) return false;

    std::vector<std::vector<uint8_t>> fields;
    size_t offset = preamble_size;
    while (offset < state.size()) {
        uint64_t tag = 0;
        size_t after_tag = 0;
        if (!read_varint(state, offset, state.size(), &tag, &after_tag)) return false;
        StateFieldRange field = find_state_field(
            state, static_cast<uint32_t>(tag >> 3), offset, state.size() - offset);
        if (!field.found || field.field_offset != offset) return false;
        size_t end = field.end_offset;
        fields.emplace_back(state.begin() + offset, state.begin() + end);
        offset = end;
    }
    std::reverse(fields.begin(), fields.end());
    state.resize(preamble_size);
    for (const auto& field : fields) {
        state.insert(state.end(), field.begin(), field.end());
    }
    refresh_state_container(state);
    return true;
}

TEST(Ear6State, RejectsUnknownSystemPayloadVersionWithoutMutation) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> before = save_state(ctx);
    ASSERT_GT(before.size(), STATE_PREAMBLE_SIZE);

    std::vector<uint8_t> future_payload = before;
    StateFieldRange payload = find_state_field(future_payload, STATE_FIELD_SYSTEM_STATE);
    ASSERT_TRUE(payload.found);
    ASSERT_GT(payload.size, 0u);
    future_payload[payload.data_offset] = 2;
    refresh_state_container(future_payload);

    EXPECT_NE(ear6_load_state_from_memory(
        ctx, future_payload.data(), future_payload.size()), 0);
    EXPECT_EQ(save_state(ctx), before);

    ear6_destroy(ctx);
}

TEST(Ear6State, SkipsUnknownOptionalContainerField) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> canonical = save_state(ctx);
    std::vector<uint8_t> extended = canonical;
    append_test_length_delimited_field(extended, 100, {0xE6, 0x01});

    Ear6StateInfo info = {};
    EXPECT_EQ(ear6_get_state_info(extended.data(), extended.size(), &info), 0);
    EXPECT_EQ(ear6_load_state_from_memory(ctx, extended.data(), extended.size()), 0);
    EXPECT_EQ(save_state(ctx), canonical);
    ear6_destroy(ctx);
}

TEST(Ear6State, RejectsMissingRequiredContainerFieldWithoutMutation) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> before = save_state(ctx);
    std::vector<uint8_t> incomplete = before;
    remove_state_field(incomplete, STATE_FIELD_SYSTEM_STATE);

    Ear6StateInfo info = {};
    EXPECT_NE(ear6_get_state_info(incomplete.data(), incomplete.size(), &info), 0);
    EXPECT_NE(ear6_load_state_from_memory(ctx, incomplete.data(), incomplete.size()), 0);
    EXPECT_EQ(save_state(ctx), before);
    ear6_destroy(ctx);
}

TEST(Ear6State, AcceptsReorderedContainerFields) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> canonical = save_state(ctx);
    std::vector<uint8_t> reordered = canonical;
    ASSERT_TRUE(reverse_state_fields(reordered));
    ASSERT_NE(reordered, canonical);

    EXPECT_EQ(ear6_load_state_from_memory(ctx, reordered.data(), reordered.size()), 0);
    EXPECT_EQ(save_state(ctx), canonical);
    ear6_destroy(ctx);
}

TEST(Ear6State, AcceptsDuplicateScalarUsingLastValue) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> before = save_state(ctx);
    std::vector<uint8_t> duplicate = before;
    append_test_varint_field(duplicate, STATE_FIELD_SYSTEM_TYPE, EAR6_SYSTEM_TEST);

    EXPECT_EQ(ear6_load_state_from_memory(ctx, duplicate.data(), duplicate.size()), 0);
    EXPECT_EQ(save_state(ctx), before);
    ear6_destroy(ctx);
}

static std::vector<uint8_t> make_mapper0_test_rom(uint8_t marker) {
    std::vector<uint8_t> rom(16 + 0x4000, 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = 1;
    rom[16] = 0xE6; rom[17] = 0x00;       // INC $00
    rom[18] = 0x4C; rom[19] = 0x00;       // JMP $8000
    rom[20] = 0x80;
    rom[16 + 0x3FFC] = 0x00;
    rom[16 + 0x3FFD] = 0x80;
    rom[16 + 0x0100] = marker;
    return rom;
}

static std::vector<uint8_t> make_mapper_test_rom(uint8_t mapper) {
    constexpr size_t PRG_SIZE = 0x8000;
    constexpr size_t CHR_SIZE = 0x2000;
    std::vector<uint8_t> rom(16 + PRG_SIZE + CHR_SIZE, 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = PRG_SIZE / 0x4000;
    rom[5] = CHR_SIZE / 0x2000;
    rom[6] = static_cast<uint8_t>((mapper & 0x0F) << 4);
    rom[7] = mapper & 0xF0;

    for (size_t bank = 0; bank < PRG_SIZE; bank += 0x2000) {
        size_t offset = 16 + bank;
        rom[offset + 0] = 0xE6; // INC $00
        rom[offset + 1] = 0x00;
        rom[offset + 2] = 0x4C; // JMP $8000
        rom[offset + 3] = 0x00;
        rom[offset + 4] = 0x80;
        rom[offset + 0x1FFC] = 0x00;
        rom[offset + 0x1FFD] = 0x80;
    }
    return rom;
}

TEST(Ear6State, RejectsCorruptNesStatesWithoutMutation) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);
    std::vector<uint8_t> rom = make_mapper0_test_rom(0x11);
    ASSERT_EQ(ear6_load_from_memory(
        ctx, rom.data(), static_cast<int>(rom.size()), nullptr), 0);
    for (int i = 0; i < 3; ++i) ASSERT_EQ(ear6_step(ctx), 0);

    std::vector<uint8_t> before = save_state(ctx);
    ASSERT_GT(before.size(), STATE_PREAMBLE_SIZE);

    std::vector<uint8_t> truncated(before.begin(), before.end() - 1);
    EXPECT_NE(ear6_load_state_from_memory(ctx, truncated.data(), truncated.size()), 0);
    EXPECT_EQ(save_state(ctx), before);

    std::vector<uint8_t> bad_crc = before;
    bad_crc.back() ^= 0x01;
    EXPECT_NE(ear6_load_state_from_memory(ctx, bad_crc.data(), bad_crc.size()), 0);
    EXPECT_EQ(save_state(ctx), before);

    // Mapper 0 payload ends with kb_enabled_, a serialized bool. Keep the body
    // CRC valid so failure happens inside the system payload loader.
    std::vector<uint8_t> invalid_payload = before;
    StateFieldRange system_state = find_state_field(
        invalid_payload, STATE_FIELD_SYSTEM_STATE);
    ASSERT_TRUE(system_state.found);
    ASSERT_GT(system_state.size, 0u);
    invalid_payload[system_state.data_offset + system_state.size - 1] = 2;
    refresh_state_container(invalid_payload);
    EXPECT_NE(ear6_load_state_from_memory(
        ctx, invalid_payload.data(), invalid_payload.size()), 0);
    EXPECT_EQ(save_state(ctx), before);

    ear6_destroy(ctx);
}

TEST(Ear6State, NesStateEmbedsContentAndNameHint) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);
    std::vector<uint8_t> rom = make_mapper0_test_rom(0x11);
    const std::string source_hint =
        "https://example.invalid/download/embedded-game.nes?token=secret";
    const std::string name_hint = "embedded-game.nes";
    ASSERT_EQ(ear6_load_from_memory(
        ctx, rom.data(), static_cast<int>(rom.size()), source_hint.c_str()), 0);

    std::vector<uint8_t> state = save_state(ctx);
    ASSERT_FALSE(state.empty());
    StateFieldRange content = find_state_field(state, STATE_FIELD_CONTENT);
    StateFieldRange stored_name = find_state_field(state, STATE_FIELD_CONTENT_NAME);
    ASSERT_TRUE(content.found);
    ASSERT_TRUE(stored_name.found);
    ASSERT_EQ(content.size, rom.size());
    ASSERT_EQ(stored_name.size, name_hint.size());
    EXPECT_EQ(std::string(
        state.begin() + stored_name.data_offset,
        state.begin() + stored_name.data_offset + stored_name.size), name_hint);
    EXPECT_TRUE(std::equal(
        rom.begin(), rom.end(),
        state.begin() + content.data_offset));

    Ear6StateInfo info = {};
    ASSERT_EQ(ear6_get_state_info(state.data(), state.size(), &info), 0);
    EXPECT_EQ(
        info.container_wire_version,
        EAR6_STATE_CONTAINER_WIRE_VERSION);
    EXPECT_EQ(info.system_type, EAR6_SYSTEM_NES);
    EXPECT_NE(info.content_identity, 0u);
    EXPECT_EQ(info.content_size, rom.size());
    ASSERT_EQ(info.content_name_hint_size, name_hint.size());
    ASSERT_NE(info.content_name_hint, nullptr);
    EXPECT_EQ(std::string(info.content_name_hint, info.content_name_hint_size), name_hint);

    ear6_destroy(ctx);
}

TEST(Ear6State, NesStateEmbedsCurrentFramePreview) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);
    std::vector<uint8_t> rom = make_mapper0_test_rom(0x11);
    ASSERT_EQ(ear6_load_from_memory(
        ctx, rom.data(), static_cast<int>(rom.size()), "preview-game.nes"), 0);
    for (int i = 0; i < 3; ++i) ASSERT_EQ(ear6_step(ctx), 0);

    const uint8_t* framebuffer = ear6_get_framebuffer(ctx);
    ASSERT_NE(framebuffer, nullptr);
    std::vector<uint8_t> expected(framebuffer, framebuffer + 256 * 240 * 4);
    std::vector<uint8_t> state = save_state(ctx);
    ASSERT_FALSE(state.empty());

    Ear6StateInfo info = {};
    ASSERT_EQ(ear6_get_state_info(state.data(), state.size(), &info), 0);
    EXPECT_EQ(info.preview_format, EAR6_STATE_PREVIEW_RGBA8888);
    EXPECT_EQ(info.preview_width, 256);
    EXPECT_EQ(info.preview_height, 240);
    ASSERT_EQ(info.preview_size, expected.size());
    ASSERT_NE(info.preview_data, nullptr);
    EXPECT_EQ(std::memcmp(info.preview_data, expected.data(), expected.size()), 0);

    ear6_destroy(ctx);
}

TEST(Ear6State, IgnoresMalformedOptionalPreview) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);
    std::vector<uint8_t> rom = make_mapper0_test_rom(0x11);
    ASSERT_EQ(ear6_load_from_memory(
        ctx, rom.data(), static_cast<int>(rom.size()), "preview-game.nes"), 0);
    ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> before = save_state(ctx);
    ASSERT_FALSE(before.empty());

    std::vector<uint8_t> malformed = before;
    StateFieldRange preview = find_state_field(malformed, STATE_FIELD_PREVIEW);
    ASSERT_TRUE(preview.found);
    StateFieldRange format = find_state_field(
        malformed, 2, preview.data_offset, preview.size);
    ASSERT_TRUE(format.found);
    ASSERT_EQ(format.wire_type, 0u);
    ASSERT_EQ(format.size, 1u);
    malformed[format.data_offset] = 99;
    refresh_state_container(malformed);

    Ear6StateInfo info = {};
    EXPECT_EQ(ear6_get_state_info(malformed.data(), malformed.size(), &info), 0);
    EXPECT_EQ(info.preview_format, EAR6_STATE_PREVIEW_NONE);
    EXPECT_EQ(info.preview_data, nullptr);
    EXPECT_EQ(ear6_load_state_from_memory(ctx, malformed.data(), malformed.size()), 0);
    EXPECT_EQ(save_state(ctx), before);

    ear6_destroy(ctx);
}

TEST(Ear6State, LoadsWithoutOptionalPreview) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(ctx, nullptr);
    ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> canonical = save_state(ctx);
    std::vector<uint8_t> without_preview = canonical;
    remove_state_field(without_preview, STATE_FIELD_PREVIEW);

    Ear6StateInfo info = {};
    ASSERT_EQ(ear6_get_state_info(
        without_preview.data(), without_preview.size(), &info), 0);
    EXPECT_EQ(info.preview_format, EAR6_STATE_PREVIEW_NONE);
    EXPECT_EQ(ear6_load_state_from_memory(
        ctx, without_preview.data(), without_preview.size()), 0);
    EXPECT_EQ(save_state(ctx), canonical);
    ear6_destroy(ctx);
}

TEST(Ear6State, SupportedNesMappersContinueDeterministically) {
    const uint8_t mappers[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 16, 17, 18, 19, 21,
        22, 23, 24, 25,
        26, 32, 33, 34, 35, 38, 39,
        40, 41, 42, 43, 45, 46, 50, 57, 58, 60, 61, 62, 64, 65, 66, 67,
        68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 86, 87, 88, 89,
        90, 92, 93,
        94, 95, 99, 101, 107, 112, 113, 117, 118, 133, 140, 143, 144, 145,
        146, 148, 149, 150, 151, 153, 154, 156, 157, 159, 170, 174, 180,
        184, 185, 200,
        202, 203, 204, 206, 210, 213, 214, 216, 221, 225, 226, 227, 229,
        230, 231, 233, 240, 241, 242, 243, 244, 245, 246, 252,
    };

    for (uint8_t mapper : mappers) {
        SCOPED_TRACE(static_cast<int>(mapper));
        Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
        ASSERT_NE(ctx, nullptr);
        std::vector<uint8_t> rom = make_mapper_test_rom(mapper);
        ASSERT_EQ(ear6_load_from_memory(
            ctx, rom.data(), static_cast<int>(rom.size()), nullptr), 0);
        ASSERT_EQ(ear6_step(ctx), 0);

        std::vector<uint8_t> checkpoint = save_state(ctx);
        ASSERT_FALSE(checkpoint.empty());
        ASSERT_EQ(ear6_step(ctx), 0);
        std::vector<uint8_t> expected = save_state(ctx);
        ASSERT_FALSE(expected.empty());

        ASSERT_EQ(ear6_load_state_from_memory(ctx, checkpoint.data(), checkpoint.size()), 0);
        ASSERT_EQ(ear6_step(ctx), 0);
        EXPECT_EQ(save_state(ctx), expected);

        Ear6* restored = ear6_create(EAR6_SYSTEM_NES);
        ASSERT_NE(restored, nullptr);
        ASSERT_EQ(ear6_load_state_from_memory(
            restored, checkpoint.data(), checkpoint.size()), 0);
        ASSERT_EQ(ear6_step(restored), 0);
        EXPECT_EQ(save_state(restored), expected);

        ear6_destroy(restored);
        ear6_destroy(ctx);
    }
}

TEST(Ear6State, RealRomMappersContinueDeterministically) {
    const char* roms[] = {
        "mapper_0/10-Yard Fight (J).nes",
        "mapper_1/'89 Dennou Kyuusei Uranai (J).nes",
        "mapper_10/Famicom Wars (J).nes",
        "mapper_117/sango4.nes",
        "mapper_118/Arumajiro (J).nes",
        "mapper_15/100IN1.NES",
        "mapper_16/Crayon Shin Chan (J).nes",
        "mapper_17/Dynamite Batman (J).nes",
        "mapper_18/Saiyuuki World 2 - Tenjoukai No Mashou (J).nes",
        "mapper_184/Toukaidou Gojuusan Tsugi (J).nes",
        "mapper_19/Battle Fleet (J).nes",
        "mapper_2/1943 (J).nes",
        "mapper_21/Ganbare Goemon Gaiden 2 - Tenka No Zaihou (J).nes",
        "mapper_22/Ganbare Penant Race! (J).nes",
        "mapper_226/Super_42-in-1.nes",
        "mapper_23/Akumajou Special - Boku Dracula Kun (J).nes",
        "mapper_24/Akumajou Densetsu (J).nes",
        "mapper_240/Gen Ke Le Zhuan (C).nes",
        "mapper_243/Poker 3 - 5 in 1 (C).nes",
        "mapper_245/Yong Ze Do Re Long 6 (C).nes",
        "mapper_25/Bio Miracle Bokutte Upa (J).nes",
        "mapper_252/3GO.NES",
        "mapper_26/Esper Dream 2 - Aratanaru Tatakai (J).nes",
        "mapper_3/ASO - Armored Scrum Object (J).nes",
        "mapper_32/Meikyuujima (J).nes",
        "mapper_33/Akira (J).nes",
        "mapper_35/Bar Games (J).nes",
        "mapper_4/1999 - Hore, Mitakotoka! Seikimatsu (J).nes",
        "mapper_41/caltron.nes",
        "mapper_45/BrainSeries13in1.nes",
        "mapper_5/Nobunaga No Yabou - Sengoku Gunyuuden (J).nes",
        "mapper_58/sag32-1.nes",
        "mapper_62/s700in1.nes",
        "mapper_64/Dig Dug 2 (J).nes",
        "mapper_65/Ai Sensei No Oshiete - Watashi No Hoshi (J).nes",
        "mapper_66/Bio Senshi Dan - Increaser To No Tatakai (J).nes",
        "mapper_67/Fantasy Zone 2 - The Teardrop of Opa-Opa (J).nes",
        "mapper_68/After Burner 2 (J).nes",
        "mapper_69/Batman (J).nes",
        "mapper_7/Battletoads Double Dragon (U).nes",
        "mapper_70/Gegege No Kitarou 2 - Youkaigundan No Chousen (J).nes",
        "mapper_73/Salamander (J).nes",
        "mapper_74/d4cjqrdz.nes",
        "mapper_75/Exciting Boxing (J).nes",
        "mapper_76/Digital Devil Monogatari - Megami Tensei (J).nes",
        "mapper_88/Devil Man (J).nes",
        "mapper_89/Mito Koumon (J).nes",
        "mapper_90/finalf3.nes",
        "mapper_93/Fantasy Zone (J).nes",
        "mapper_95/Dragon Buster (J).nes",
        "mapper_99/vs battle city.nes",
    };

    int exercised = 0;
    for (const char* relative_path : roms) {
        SCOPED_TRACE(relative_path);
        std::string path = std::string(EAR6_SOURCE_DIR)
            + "/tests/local-roms/nes/" + relative_path;
        FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) continue;
        std::fclose(file);
        ++exercised;

        Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
        ASSERT_NE(ctx, nullptr);
        ASSERT_EQ(ear6_load_from_file(ctx, path.c_str()), 0);
        for (int i = 0; i < 60; ++i) ASSERT_EQ(ear6_step(ctx), 0);

        std::vector<uint8_t> checkpoint = save_state(ctx);
        ASSERT_FALSE(checkpoint.empty());
        for (int i = 0; i < 3; ++i) ASSERT_EQ(ear6_step(ctx), 0);
        std::vector<uint8_t> expected = save_state(ctx);
        ASSERT_FALSE(expected.empty());

        ASSERT_EQ(ear6_load_state_from_memory(ctx, checkpoint.data(), checkpoint.size()), 0);
        for (int i = 0; i < 3; ++i) ASSERT_EQ(ear6_step(ctx), 0);
        EXPECT_EQ(save_state(ctx), expected);

        Ear6* restored = ear6_create(EAR6_SYSTEM_NES);
        ASSERT_NE(restored, nullptr);
        ASSERT_EQ(ear6_load_state_from_memory(
            restored, checkpoint.data(), checkpoint.size()), 0);
        for (int i = 0; i < 3; ++i) ASSERT_EQ(ear6_step(restored), 0);
        EXPECT_EQ(save_state(restored), expected);

        ear6_destroy(restored);
        ear6_destroy(ctx);
    }
    EXPECT_GT(exercised, 0);
}

TEST(Ear6State, NesMapper0ContinuationIsDeterministic) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(ctx, nullptr);
    std::vector<uint8_t> rom = make_mapper0_test_rom(0x11);
    ASSERT_EQ(ear6_load_from_memory(ctx, rom.data(), static_cast<int>(rom.size()), nullptr), 0);
    for (int i = 0; i < 3; ++i) ASSERT_EQ(ear6_step(ctx), 0);

    std::vector<uint8_t> checkpoint = save_state(ctx);
    ASSERT_FALSE(checkpoint.empty());
    for (int i = 0; i < 4; ++i) ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> expected = save_state(ctx);
    ASSERT_FALSE(expected.empty());

    ASSERT_EQ(ear6_load_state_from_memory(ctx, checkpoint.data(), checkpoint.size()), 0);
    for (int i = 0; i < 4; ++i) ASSERT_EQ(ear6_step(ctx), 0);
    std::vector<uint8_t> actual = save_state(ctx);
    ASSERT_EQ(actual.size(), expected.size());
    auto mismatch = std::mismatch(
        actual.begin(),
        actual.end(),
        expected.begin()
    );
    ASSERT_EQ(mismatch.first, actual.end())
        << "first state mismatch at byte " << (mismatch.first - actual.begin())
        << ": actual=" << static_cast<int>(*mismatch.first)
        << " expected=" << static_cast<int>(*mismatch.second);

    ear6_destroy(ctx);
}

TEST(Ear6State, EmbeddedContentReplacesDifferentNesContent) {
    std::vector<uint8_t> first_rom = make_mapper0_test_rom(0x11);
    std::vector<uint8_t> second_rom = make_mapper0_test_rom(0x22);
    Ear6* first = ear6_create(EAR6_SYSTEM_NES);
    Ear6* second = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(ear6_load_from_memory(first, first_rom.data(), static_cast<int>(first_rom.size()), nullptr), 0);
    ASSERT_EQ(ear6_load_from_memory(second, second_rom.data(), static_cast<int>(second_rom.size()), nullptr), 0);

    std::vector<uint8_t> state = save_state(first);
    ASSERT_FALSE(state.empty());
    ASSERT_EQ(ear6_load_state_from_memory(second, state.data(), state.size()), 0);
    EXPECT_EQ(save_state(second), state);

    ear6_destroy(first);
    ear6_destroy(second);
}

TEST(Ear6State, LoadsEmbeddedNesContentIntoEmptyContext) {
    std::vector<uint8_t> rom = make_mapper0_test_rom(0x11);
    Ear6* loaded = ear6_create(EAR6_SYSTEM_NES);
    Ear6* empty = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(loaded, nullptr);
    ASSERT_NE(empty, nullptr);
    ASSERT_EQ(ear6_load_from_memory(
        loaded, rom.data(), static_cast<int>(rom.size()), nullptr), 0);

    std::vector<uint8_t> state = save_state(loaded);
    ASSERT_FALSE(state.empty());
    ASSERT_EQ(ear6_load_state_from_memory(empty, state.data(), state.size()), 0);
    EXPECT_EQ(save_state(empty), state);

    ear6_destroy(loaded);
    ear6_destroy(empty);
}

TEST(Ear6State, RejectsModifiedEmbeddedContentWithoutMutation) {
    std::vector<uint8_t> rom = make_mapper0_test_rom(0x11);
    Ear6* source = ear6_create(EAR6_SYSTEM_NES);
    Ear6* current = ear6_create(EAR6_SYSTEM_NES);
    ASSERT_NE(source, nullptr);
    ASSERT_NE(current, nullptr);
    ASSERT_EQ(ear6_load_from_memory(
        source, rom.data(), static_cast<int>(rom.size()), nullptr), 0);
    std::vector<uint8_t> other_rom = make_mapper0_test_rom(0x22);
    ASSERT_EQ(ear6_load_from_memory(
        current, other_rom.data(), static_cast<int>(other_rom.size()), nullptr), 0);

    std::vector<uint8_t> state = save_state(source);
    std::vector<uint8_t> before = save_state(current);
    StateFieldRange content = find_state_field(state, STATE_FIELD_CONTENT);
    ASSERT_TRUE(content.found);
    ASSERT_GT(content.size, 0x0100u);
    state[content.data_offset + 0x0100] ^= 0x01;
    refresh_state_container(state);

    EXPECT_NE(ear6_load_state_from_memory(current, state.data(), state.size()), 0);
    EXPECT_EQ(save_state(current), before);

    ear6_destroy(source);
    ear6_destroy(current);
}

TEST(Ear6State, RejectsStateForIncompatibleTargetSystemWithoutMutation) {
    std::vector<uint8_t> rom = make_mapper0_test_rom(0x11);
    Ear6* nes = ear6_create(EAR6_SYSTEM_NES);
    Ear6* test = ear6_create(EAR6_SYSTEM_TEST);
    ASSERT_NE(nes, nullptr);
    ASSERT_NE(test, nullptr);
    ASSERT_EQ(ear6_load_from_memory(
        nes, rom.data(), static_cast<int>(rom.size()), nullptr), 0);
    ASSERT_EQ(ear6_step(test), 0);

    std::vector<uint8_t> nes_state = save_state(nes);
    std::vector<uint8_t> before = save_state(test);
    ASSERT_FALSE(nes_state.empty());
    ASSERT_FALSE(before.empty());
    EXPECT_NE(ear6_load_state_from_memory(
        test, nes_state.data(), nes_state.size()), 0);
    EXPECT_EQ(save_state(test), before);

    ear6_destroy(nes);
    ear6_destroy(test);
}
