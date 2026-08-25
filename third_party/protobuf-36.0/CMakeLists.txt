cmake_minimum_required(VERSION 3.17)

# Ear6 vendors only the upb runtime needed by checked-in generated messages.
# Keep this list explicit so adding protobuf features cannot silently grow the
# native or WebAssembly binaries.
add_library(ear6_upb OBJECT
    third_party/utf8_range/utf8_range.c
    upb/hash/common.c
    upb/mem/alloc.c
    upb/mem/arena.c
    upb/message/accessors.c
    upb/message/array.c
    upb/message/compat.c
    upb/message/internal/extension.c
    upb/message/internal/iterator.c
    upb/message/internal/message.c
    upb/message/map.c
    upb/message/map_sorter.c
    upb/message/message.c
    upb/message/unknown_fields.c
    upb/mini_table/extension_registry.c
    upb/mini_table/generated_registry.c
    upb/mini_table/internal/message.c
    upb/mini_table/message.c
    upb/port/port.c
    upb/wire/decode.c
    upb/wire/encode.c
    upb/wire/eps_copy_input_stream.c
    upb/wire/internal/back_alloc.c
    upb/wire/internal/decoder.c
    upb/wire/internal/encoder.c
    upb/wire/reader.c
)

target_include_directories(ear6_upb SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/utf8_range
)

set_target_properties(ear6_upb PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
    target_compile_options(ear6_upb PRIVATE /Zc:preprocessor)
endif()
