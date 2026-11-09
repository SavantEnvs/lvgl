/* Plays the first frames of an in-memory GIF through the lv_gif widget (AnimatedGIF LZW decoder). */
#include "fuzz_common.h"

/* Harness resource bound, not a library contract: a header-declared canvas above this only measures the
 * allocator (libFuzzer's rss limit reports it as OOM), so such inputs are rejected with -1, not run. */
#define MAX_PIXELS (1024u * 1024u)

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    fuzz_lvgl_init();

    /* The logical screen size sits at bytes 6..9 of the header. */
    if(size >= 10) {
        uint32_t w = data[6] | (data[7] << 8);
        uint32_t h = data[8] | (data[9] << 8);
        if(w * h > MAX_PIXELS) return -1;
    }

    uint8_t * buf = fuzz_copy(data, size);
    lv_image_dsc_t img;
    memset(&img, 0, sizeof(img));
    img.header.magic = LV_IMAGE_HEADER_MAGIC;
    img.header.cf = LV_COLOR_FORMAT_RAW;
    img.data_size = size;
    img.data = buf;

    lv_obj_t * gif = lv_gif_create(lv_screen_active());
    lv_gif_set_src(gif, &img);
    for(int i = 0; i < 4; i++) {
        lv_tick_inc(200);
        lv_timer_handler();
    }
    lv_obj_delete(gif);

    free(buf);
    return 0;
}
