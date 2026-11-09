/* Loads an LVGL binary font (lv_font_conv "bin" format) from memory and renders sample glyphs. */
#include "fuzz_common.h"
#include "lvgl/font/lv_font_fmt_txt.h"

/* Returns -1 when the glyph box is too large for the harness's own bitmap buffer (the harness allocates
 * box_w*box_h bytes for lv_font_get_glyph_bitmap(); this bounds the harness's allocation, not LVGL's). */
static int probe_glyph(const lv_font_t * font, uint32_t letter, uint32_t next)
{
    int rc = 0;
    lv_font_glyph_dsc_t g;
    if(!lv_font_get_glyph_dsc(font, &g, letter, next)) return 0;
    if(g.box_w && g.box_h) {
        if((uint32_t)g.box_w * g.box_h <= 64u * 1024u) {
            lv_draw_buf_t * db = lv_draw_buf_create(g.box_w, g.box_h, LV_COLOR_FORMAT_A8, 0);
            if(db) {
                if(lv_font_get_glyph_bitmap(&g, db)) fuzz_touch(db->data, db->data_size);
                lv_draw_buf_destroy(db);
            }
        }
        else rc = -1;
    }
    lv_font_glyph_release_draw_data(&g);
    return rc;
}

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    fuzz_lvgl_init();

    uint8_t * buf = fuzz_copy(data, size);
    int rc = 0;
    lv_font_t * font = lv_binfont_create_from_buffer(buf, (uint32_t)size);
    if(font) {
        static const uint32_t ascii[] = {' ', 'A', 'g', '0', '~', 0x00E9};
        for(unsigned i = 0; i < sizeof(ascii) / sizeof(ascii[0]); i++) rc |= probe_glyph(font, ascii[i], ascii[(i + 1) % 6]);

        const lv_font_fmt_txt_dsc_t * fdsc = (const lv_font_fmt_txt_dsc_t *)font->dsc;
        if(fdsc && fdsc->cmaps) {
            for(unsigned i = 0; i < fdsc->cmap_num && i < 8; i++) {
                const lv_font_fmt_txt_cmap_t * c = &fdsc->cmaps[i];
                uint32_t last = c->range_start + (c->range_length ? c->range_length - 1u : 0u);
                rc |= probe_glyph(font, c->range_start, 0);
                rc |= probe_glyph(font, c->range_start + 1, c->range_start);
                rc |= probe_glyph(font, c->range_start + c->range_length / 2u, 0);
                rc |= probe_glyph(font, last, 0);
            }
        }
        lv_binfont_destroy(font);
    }
    free(buf);
    return rc;
}
