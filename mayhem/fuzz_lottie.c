/* Parses a Lottie JSON animation with ThorVG's loader and renders a few frames through lv_lottie. */
#include "fuzz_common.h"

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    fuzz_lvgl_init();

    static uint8_t pixels[32 * 32 * 4 + LV_DRAW_BUF_ALIGN];
    lv_obj_t * lottie = lv_lottie_create(lv_screen_active());
    lv_lottie_set_buffer(lottie, 32, 32, lv_draw_buf_align(pixels, LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED));
    lv_lottie_set_src_data(lottie, data, size);
    for(int i = 0; i < 3; i++) {
        lv_tick_inc(150);
        lv_timer_handler();
    }
    lv_obj_delete(lottie);
    return 0;
}
