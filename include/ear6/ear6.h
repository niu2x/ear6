#pragma once

#include <ear6/export.h>
#include <ear6/version.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ear6 Ear6;

typedef enum {
    EAR6_SYSTEM_TEST,
    EAR6_SYSTEM_NES,
    EAR6_SYSTEM_FLASH,
} Ear6SystemType;

typedef void (*Ear6FrameCallback)(const void* data, int width, int height, void* user_data);

typedef void (*Ear6AudioCallback)(const int16_t* data, int num_samples, void* user_data);

EAR6_API Ear6* ear6_create(Ear6SystemType system);
EAR6_API void ear6_destroy(Ear6* ctx);

EAR6_API int ear6_load(Ear6* ctx, const char* path);
EAR6_API int ear6_load_data(Ear6* ctx, const void* data, int size, const char* name_hint);

EAR6_API void ear6_set_frame_callback(Ear6* ctx, Ear6FrameCallback cb, void* user_data);
EAR6_API void ear6_set_audio_callback(Ear6* ctx, Ear6AudioCallback cb, void* user_data);

EAR6_API int ear6_step(Ear6* ctx);

EAR6_API const uint8_t* ear6_get_framebuffer(Ear6* ctx);
EAR6_API int ear6_get_frame_width(Ear6* ctx);
EAR6_API int ear6_get_frame_height(Ear6* ctx);
EAR6_API const int16_t* ear6_get_audiobuffer(Ear6* ctx);
EAR6_API int ear6_get_audio_num_samples(Ear6* ctx);

EAR6_API int ear6_test(void);

#ifdef __cplusplus
}
#endif
