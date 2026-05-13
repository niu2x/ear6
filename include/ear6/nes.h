#pragma once

#include <ear6/ear6.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EAR6_NES_REGION_NTSC,
    EAR6_NES_REGION_PAL,
} Ear6NesRegion;

typedef enum {
    EAR6_NES_BUTTON_A      = 0,
    EAR6_NES_BUTTON_B      = 1,
    EAR6_NES_BUTTON_SELECT = 2,
    EAR6_NES_BUTTON_START  = 3,
    EAR6_NES_BUTTON_UP     = 4,
    EAR6_NES_BUTTON_DOWN   = 5,
    EAR6_NES_BUTTON_LEFT   = 6,
    EAR6_NES_BUTTON_RIGHT  = 7,
} Ear6NesButton;

EAR6_API int ear6_nes_set_region(Ear6* ctx, Ear6NesRegion region);
EAR6_API int ear6_nes_set_mapper(Ear6* ctx, int mapper_number);
EAR6_API int ear6_nes_set_palette(Ear6* ctx, const uint32_t palette[64]);
EAR6_API int ear6_nes_set_button_state(Ear6* ctx, Ear6NesButton button, int pressed);
EAR6_API void ear6_nes_clear_input(Ear6* ctx);

#ifdef __cplusplus
}
#endif
