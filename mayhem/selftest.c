/* Oracle self-test: known-answer checks over LVGL's decoders, font loaders and text engine.
 * Built with the project's normal flags and run from the repo root by mayhem/test.sh. */
#include "fuzz_common.h"
#include <stdio.h>

#define EXPECTED_CHECKS 49

static int n_pass, n_fail;

static void check(const char * what, uint64_t got, uint64_t want)
{
    if(got == want) {
        n_pass++;
    }
    else {
        n_fail++;
        printf("FAIL %s: got 0x%llx (%llu), want 0x%llx (%llu)\n", what, (unsigned long long)got, (unsigned long long)got,
               (unsigned long long)want, (unsigned long long)want);
    }
}

static uint64_t fnv(const uint8_t * p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for(size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

static uint8_t * slurp(const char * path, size_t * len)
{
    FILE * f = fopen(path, "rb");
    if(!f) { printf("FAIL cannot open %s\n", path); n_fail++; *len = 0; return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t * b = (uint8_t *)malloc(n ? n : 1);
    if(fread(b, 1, n, f) != (size_t)n) n_fail++;
    fclose(f);
    *len = (size_t)n;
    return b;
}

/* Decode a raw image file; check width, height and a hash of the decoded pixels. */
static void check_image(const char * path, const char * memfs_ext, uint32_t w, uint32_t h, uint64_t hash)
{
    size_t len;
    uint8_t * buf = slurp(path, &len);
    if(!buf) return;
    lv_image_dsc_t img;
    memset(&img, 0, sizeof(img));
    img.header.magic = LV_IMAGE_HEADER_MAGIC;
    img.header.cf = LV_COLOR_FORMAT_RAW;
    img.data_size = len;
    img.data = buf;

    lv_fs_path_ex_t mpath;
    const void * src = &img;
    if(memfs_ext) {
        lv_fs_make_path_from_buffer(&mpath, LV_FS_MEMFS_LETTER, buf, (uint32_t)len, memfs_ext);
        src = &mpath;
    }
    lv_image_header_t header;
    memset(&header, 0, sizeof(header));
    lv_result_t r = lv_image_decoder_get_info(src, &header);
    check(path, r == LV_RESULT_OK, 1);
    check(path, ((uint64_t)header.w << 32) | header.h, ((uint64_t)w << 32) | h);

    lv_image_decoder_dsc_t dsc;
    lv_image_decoder_args_t args;
    memset(&args, 0, sizeof(args));
    args.no_cache = true;
    uint64_t got = 0;
    lv_result_t orc = lv_image_decoder_open(&dsc, src, &args);
    check(path, orc == LV_RESULT_OK, 1);
    if(orc == LV_RESULT_OK) {
        if(!dsc.decoded && memfs_ext && memfs_ext[0] == 'j') fuzz_read_rows(&dsc);
        if(dsc.decoded) got = fnv(dsc.decoded->data, dsc.decoded->data_size);
        lv_image_decoder_close(&dsc);
    }
    check(path, got, hash);
    free(buf);
}

static void check_font(const char * path, uint32_t letter, uint32_t adv, uint32_t box, uint64_t hash)
{
    size_t len;
    uint8_t * buf = slurp(path, &len);
    if(!buf) return;
    lv_font_t * font = lv_binfont_create_from_buffer(buf, (uint32_t)len);
    check(path, font != NULL, 1);
    if(font) {
        lv_font_glyph_dsc_t g;
        memset(&g, 0, sizeof(g));
        check(path, lv_font_get_glyph_dsc(font, &g, letter, 0), 1);
        check(path, ((uint64_t)g.adv_w << 32) | ((uint64_t)g.box_w << 16) | g.box_h, ((uint64_t)adv << 32) | box);
        uint64_t got = 0;
        if(g.box_w && g.box_h) {
            lv_draw_buf_t * db = lv_draw_buf_create(g.box_w, g.box_h, LV_COLOR_FORMAT_A8, 0);
            if(db) {
                if(lv_font_get_glyph_bitmap(&g, db)) got = fnv(db->data, db->data_size);
                lv_draw_buf_destroy(db);
            }
        }
        check(path, got, hash);
        lv_font_glyph_release_draw_data(&g);
        lv_binfont_destroy(font);
    }
    free(buf);
}

int main(void)
{
    fuzz_lvgl_init();

    /* Arabic presentation-form shaping (from LVGL's own test_text_ap.c) */
    {
        char in[] = "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7";
        static const uint8_t want[] = {0xEF, 0xBB, 0xA3, 0xEF, 0xBA, 0xAE, 0xEF, 0xBA, 0xA3, 0xEF, 0xBA, 0x92, 0xEF, 0xBA, 0x8E};
        check("ap_strlen", lv_text_ap_strlen(in), 15);
        check("ap_strlen_alef", lv_text_ap_strlen("\xD8\xA2"), 3);
        char out[32];
        memset(out, 0, sizeof(out));
        lv_text_ap_proc(in, out);
        check("ap_proc_len", strlen(out), sizeof(want));
        check("ap_proc_bytes", fnv((const uint8_t *)out, strlen(out)), fnv(want, sizeof(want)));
        check("ap_proc_memcmp", memcmp(out, want, sizeof(want)) == 0, 1);
    }

    /* Bidi: RTL paragraph is reordered visually (from LVGL's test_bidi.c) */
    {
        char out[8];
        memset(out, 0, sizeof(out));
        lv_bidi_process_paragraph("\xd8\xb9\xd8\xb1", out, 4, LV_BASE_DIR_RTL, NULL, 0);
        check("bidi_rtl", memcmp(out, "\xd8\xb1\xd8\xb9", 4) == 0, 1);
        check("bidi_detect_ltr", lv_bidi_detect_base_dir("Hello"), LV_BASE_DIR_LTR);
        check("bidi_detect_rtl", lv_bidi_detect_base_dir("\xd8\xb9\xd8\xb1"), LV_BASE_DIR_RTL);
    }

    /* Raster images */
    check_image("mayhem/fuzz_image_raw/testsuite/test_img_lvgl_logo.png", NULL, 105, 40, 0x68817eca2b776b51ull);
    check_image("mayhem/fuzz_image_raw/testsuite/test_img_lvgl_logo.bmp", "bmp", 105, 40, 0 /* BMP rows stream through get_area; open() leaves decoded NULL */);
    check_image("mayhem/fuzz_image_raw/testsuite/test_img_lvgl_logo.jpg", "jpg", 105, 40, 0x85b2122728f81862ull);
    check_image("mayhem/fuzz_image_raw/testsuite/test_arc_bg.png", NULL, 100, 100, 0x76c434633b55609bull);

    check_image("mayhem/fuzz_image_lvimg/testsuite/raw_RGB565.lvimg", "bin", 100, 100, 0x55139a53273ded3bull);
    check_image("mayhem/fuzz_image_lvimg/testsuite/lz4_RGB565.lvimg", "bin", 100, 100, 0x8f23d753bfa4e7d6ull);
    check_image("mayhem/fuzz_image_lvimg/testsuite/lz4_I8.lvimg", "bin", 100, 100, 0xce57e96e45aeaf5full);

    /* SVG document tree */
    {
        const char * svg = "<svg width=\"10\" height=\"20\"><rect x=\"1\" y=\"2\" width=\"3\" height=\"4\"/><circle cx=\"5\" cy=\"5\" r=\"2\"/></svg>";
        lv_svg_node_t * doc = lv_svg_load_data(svg, (uint32_t)strlen(svg));
        check("svg_parse", doc != NULL, 1);
        if(doc) {
            check("svg_root_children", doc->base.child_cnt, 2);
            lv_svg_node_delete(doc);
        }
    }

    /* Tiny TTF */
    {
        size_t len;
        uint8_t * buf = slurp("tests/src/test_assets/test_gpos_one.ttf", &len);
        lv_font_t * font = buf ? lv_tiny_ttf_create_data(buf, len, 20) : NULL;
        check("ttf_create", font != NULL, 1);
        if(font) {
            lv_font_glyph_dsc_t g;
            memset(&g, 0, sizeof(g));
            check("ttf_glyph", lv_font_get_glyph_dsc(font, &g, 'A', 0), 1);
            check("ttf_A", ((uint64_t)g.adv_w << 32) | ((uint64_t)g.box_w << 16) | g.box_h, 0xd000f000eull);
            lv_font_glyph_release_draw_data(&g);
            lv_tiny_ttf_destroy(font);
        }
        free(buf);
    }

    /* Binary fonts */
    check_font("mayhem/fuzz_font_fnt/testsuite/test_font_1.fnt", 'A', 6, (7 << 16) | 5, 0x863d5c1ae5215512ull);
    check_font("mayhem/fuzz_font_fnt/testsuite/test_font_2.fnt", 'g', 6, (5 << 16) | 5, 0x96d83532b270a8bcull);

    printf("selftest: %d passed, %d failed\n", n_pass, n_fail);
    if(n_fail) return 1;
    if(n_pass != EXPECTED_CHECKS) { printf("selftest: expected %d checks\n", EXPECTED_CHECKS); return 1; }
    return 0;
}
