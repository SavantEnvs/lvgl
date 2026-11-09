/* Feeds an in-memory LVGL ".bin" image (12-byte header, raw/RLE/LZ4 pixel data) to the bin decoder. */
#include "fuzz_common.h"

/* Harness resource bound, not a library contract: a header-declared canvas above this only measures the
 * allocator (libFuzzer's rss limit reports it as OOM), so such inputs are rejected with -1, not run. */
#define MAX_PIXELS (4u * 1024u * 1024u)

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    fuzz_lvgl_init();

    uint8_t * buf = fuzz_copy(data, size);
    lv_fs_path_ex_t path;
    lv_fs_make_path_from_buffer(&path, LV_FS_MEMFS_LETTER, buf, (uint32_t)size, "bin");

    lv_image_header_t header;
    if(lv_image_decoder_get_info(&path, &header) == LV_RESULT_OK) {
        if((uint64_t)header.w * header.h > MAX_PIXELS) {
            free(buf);
            return -1;
        }
        lv_image_decoder_dsc_t dsc;
        lv_image_decoder_args_t args;
        memset(&args, 0, sizeof(args));
        args.no_cache = true;
        if(lv_image_decoder_open(&dsc, &path, &args) == LV_RESULT_OK) {
            if(dsc.decoded) fuzz_touch(dsc.decoded->data, dsc.decoded->data_size);
            lv_image_decoder_close(&dsc);
        }
    }

    free(buf);
    return 0;
}
