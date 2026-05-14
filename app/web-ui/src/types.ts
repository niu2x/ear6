export type SystemType = 'nes' | 'test' | 'flash'

export const SYSTEM_IDS: Record<SystemType, number> = {
  nes: 1,
  test: 0,
  flash: 2,
}

export const SYSTEM_LABELS: Record<SystemType, string> = {
  nes: 'NES',
  test: 'Test',
  flash: 'Flash',
}

export interface Ear6Module {
  HEAPU8: Uint8Array
  _ear6_web_create(system: number): number
  _ear6_web_destroy(ctx: number): void
  _ear6_web_load(ctx: number, ptr: number, size: number): number
  _ear6_web_step(ctx: number): number
  _ear6_web_get_framebuffer(ctx: number): number
  _ear6_web_get_frame_width(ctx: number): number
  _ear6_web_get_frame_height(ctx: number): number
  _ear6_web_nes_set_region(ctx: number, region: number): number
  _ear6_web_nes_set_button_state(ctx: number, button: number, pressed: number): void
  _ear6_web_nes_clear_input(ctx: number): void
  _ear6_web_get_audiobuffer(ctx: number): number
  _ear6_web_get_audio_num_samples(ctx: number): number
  _ear6_web_consume_audio(ctx: number): void
  _malloc(size: number): number
  _free(ptr: number): void
}

declare global {
  interface Window {
    createEar6: () => Promise<Ear6Module>
  }
}
