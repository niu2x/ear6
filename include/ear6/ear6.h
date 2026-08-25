#pragma once

#include <ear6/export.h>
#include <ear6/version.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ear6 Ear6;

#define EAR6_STATE_CONTAINER_WIRE_VERSION 1u

typedef enum {
    EAR6_SYSTEM_TEST,
    EAR6_SYSTEM_NES,
    EAR6_SYSTEM_FLASH,
} Ear6SystemType;

typedef enum {
    EAR6_STATE_PREVIEW_NONE,
    EAR6_STATE_PREVIEW_RGBA8888,
} Ear6StatePreviewFormat;

typedef struct {
    uint32_t container_wire_version;
    Ear6SystemType system_type;
    uint64_t content_identity;
    uint64_t content_size;
    const char* content_name_hint;
    size_t content_name_hint_size;
    Ear6StatePreviewFormat preview_format;
    const uint8_t* preview_data;
    size_t preview_size;
    int preview_width;
    int preview_height;
} Ear6StateInfo;

typedef void (*Ear6FrameCallback)(const void* data, int width, int height, void* user_data);

typedef void (*Ear6AudioCallback)(const int16_t* data, int num_samples, void* user_data);

EAR6_API Ear6* ear6_create(Ear6SystemType system);
EAR6_API void ear6_destroy(Ear6* ctx);

EAR6_API int ear6_load_from_file(Ear6* ctx, const char* path);
EAR6_API int ear6_load_from_memory(Ear6* ctx, const void* data, int size, const char* name_hint);

EAR6_API int ear6_save_state_to_memory(
    Ear6* ctx,
    void* buffer,
    size_t capacity,
    size_t* state_size
);
EAR6_API int ear6_load_state_from_memory(Ear6* ctx, const void* data, size_t size);
EAR6_API int ear6_get_state_info(const void* data, size_t size, Ear6StateInfo* info);

EAR6_API void ear6_set_frame_callback(Ear6* ctx, Ear6FrameCallback cb, void* user_data);
EAR6_API void ear6_set_audio_callback(Ear6* ctx, Ear6AudioCallback cb, void* user_data);

EAR6_API int ear6_step(Ear6* ctx);

EAR6_API const uint8_t* ear6_get_framebuffer(Ear6* ctx);
EAR6_API int ear6_get_frame_width(Ear6* ctx);
EAR6_API int ear6_get_frame_height(Ear6* ctx);
EAR6_API const int16_t* ear6_get_audiobuffer(Ear6* ctx);
EAR6_API int ear6_get_audio_num_samples(Ear6* ctx);
EAR6_API void ear6_consume_audio(Ear6* ctx);

EAR6_API int ear6_test(void);

#ifdef __cplusplus
}
#endif
