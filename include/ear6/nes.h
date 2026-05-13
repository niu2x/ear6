#pragma once

#include <ear6/ear6.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EAR6_NES_REGION_NTSC,
    EAR6_NES_REGION_PAL,
} Ear6NesRegion;

EAR6_API int ear6_nes_set_region(Ear6* ctx, Ear6NesRegion region);
EAR6_API int ear6_nes_set_mapper(Ear6* ctx, int mapper_number);
EAR6_API int ear6_nes_set_palette(Ear6* ctx, const uint32_t palette[64]);

#ifdef __cplusplus
}
#endif
