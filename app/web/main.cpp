#include <ear6/ear6.h>
#include <ear6/nes.h>
#include <emscripten.h>

extern "C" {

EMSCRIPTEN_KEEPALIVE
Ear6* ear6_web_create(int system) {
    return ear6_create(static_cast<Ear6SystemType>(system));
}

EMSCRIPTEN_KEEPALIVE
void ear6_web_destroy(Ear6* ctx) {
    ear6_destroy(ctx);
}

EMSCRIPTEN_KEEPALIVE
int ear6_web_load(Ear6* ctx, const void* data, int size) {
    return ear6_load_data(ctx, data, size, nullptr);
}

EMSCRIPTEN_KEEPALIVE
int ear6_web_load_data(Ear6* ctx, const void* data, int size, const char* name_hint) {
    return ear6_load_data(ctx, data, size, name_hint);
}

EMSCRIPTEN_KEEPALIVE
int ear6_web_step(Ear6* ctx) {
    return ear6_step(ctx);
}

EMSCRIPTEN_KEEPALIVE
const uint8_t* ear6_web_get_framebuffer(Ear6* ctx) {
    return ear6_get_framebuffer(ctx);
}

EMSCRIPTEN_KEEPALIVE
int ear6_web_get_frame_width(Ear6* ctx) {
    return ear6_get_frame_width(ctx);
}

EMSCRIPTEN_KEEPALIVE
int ear6_web_get_frame_height(Ear6* ctx) {
    return ear6_get_frame_height(ctx);
}


EMSCRIPTEN_KEEPALIVE
void ear6_web_nes_set_button_state(Ear6* ctx, int button, int pressed) {
    ear6_nes_set_button_state(ctx, static_cast<Ear6NesButton>(button), pressed);
}

EMSCRIPTEN_KEEPALIVE
void ear6_web_nes_clear_input(Ear6* ctx) {
    ear6_nes_clear_input(ctx);
}

EMSCRIPTEN_KEEPALIVE
const int16_t* ear6_web_get_audiobuffer(Ear6* ctx) {
    return ear6_get_audiobuffer(ctx);
}

EMSCRIPTEN_KEEPALIVE
int ear6_web_get_audio_num_samples(Ear6* ctx) {
    return ear6_get_audio_num_samples(ctx);
}

EMSCRIPTEN_KEEPALIVE
void ear6_web_consume_audio(Ear6* ctx) {
    ear6_consume_audio(ctx);
}

}

int main() {
    return 0;
}
