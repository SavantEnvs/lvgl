#ifndef FUZZ_COMMON_H
#define FUZZ_COMMON_H

#include "lvgl.h"
#include "lvgl_private.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void fuzz_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    (void)area; (void)px_map;
    lv_display_flush_ready(disp);
}

/* One headless 64x64 display so decoders, widgets and the draw units have a default display. */
static void fuzz_lvgl_init(void)
{
    static int done;
    if(done) return;
    done = 1;
    lv_init();
    static uint8_t buf[64 * 64 * 4] __attribute__((aligned(64)));
    lv_display_t * disp = lv_display_create(64, 64);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, fuzz_flush_cb);
}

/* Exact-size heap copy so ASan sees any read past the end of the fuzz input. */
static uint8_t * fuzz_copy(const uint8_t * data, size_t size)
{
    uint8_t * p = (uint8_t *)malloc(size ? size : 1);
    if(size) memcpy(p, data, size);
    return p;
}

static volatile uint8_t fuzz_sink;

static void fuzz_touch(const uint8_t * p, size_t n)
{
    uint8_t acc = 0;
    if(!p) return;
    for(size_t i = 0; i < n; i++) acc ^= p[i];
    fuzz_sink = acc;
}

/* Streaming decoders (JPEG) hand out rows through get_area_cb; pull a few so that path runs too. */
static void fuzz_read_rows(lv_image_decoder_dsc_t * dsc)
{
    if(dsc->decoded || !dsc->decoder || !dsc->decoder->get_area_cb) return;
    lv_area_t full, part;
    lv_area_set(&full, 0, 0, dsc->header.w - 1, dsc->header.h - 1);
    part.x1 = part.x2 = part.y2 = 0;
    part.y1 = LV_COORD_MIN;
    for(int i = 0; i < 4; i++) {
        if(dsc->decoder->get_area_cb(dsc->decoder, dsc, &full, &part) != LV_RESULT_OK) break;
        if(dsc->decoded) fuzz_touch(dsc->decoded->data, dsc->decoded->data_size);
    }
}

#endif
