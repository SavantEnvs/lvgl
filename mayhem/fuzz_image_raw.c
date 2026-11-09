/* Feeds an in-memory PNG/JPEG/BMP/SVG file to LVGL's image decoder chain, both as a raw
 * variable and through the in-memory filesystem under the extension its magic bytes imply. */
#include "fuzz_common.h"

/* Harness resource bound, not a library contract: a header-declared canvas above this only measures the
 * allocator (libFuzzer's rss limit reports it as OOM), so such inputs are rejected with -1, not run. */
#define MAX_PIXELS (4u * 1024u * 1024u)

static int decode(const void * src)
{
    lv_image_header_t header;
    if(lv_image_decoder_get_info(src, &header) != LV_RESULT_OK) return 0;
    if((uint64_t)header.w * header.h > MAX_PIXELS) return -1;
    lv_image_decoder_dsc_t dsc;
    lv_image_decoder_args_t args;
    memset(&args, 0, sizeof(args));
    args.no_cache = true;
    if(lv_image_decoder_open(&dsc, src, &args) == LV_RESULT_OK) {
        if(dsc.decoded) fuzz_touch(dsc.decoded->data, dsc.decoded->data_size);
        fuzz_read_rows(&dsc);
        lv_image_decoder_close(&dsc);
    }
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    fuzz_lvgl_init();

    uint8_t * buf = fuzz_copy(data, size);
    lv_image_dsc_t img;
    memset(&img, 0, sizeof(img));
    img.header.magic = LV_IMAGE_HEADER_MAGIC;
    img.header.cf = LV_COLOR_FORMAT_RAW;
    img.data_size = size;
    img.data = buf;
    int rc = decode(&img);

    const char * ext = NULL;
    if(size > 2 && buf[0] == 'B' && buf[1] == 'M') ext = "bmp";
    else if(size > 2 && buf[0] == 0xFF && buf[1] == 0xD8) ext = "jpg";
    if(ext) {
        lv_fs_path_ex_t path;
        lv_fs_make_path_from_buffer(&path, LV_FS_MEMFS_LETTER, buf, (uint32_t)size, ext);
        if(decode(&path) < 0) rc = -1;
    }

    free(buf);
    return rc;
}
