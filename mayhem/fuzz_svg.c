/* Parses SVG markup (XML tokenizer, style/path/transform parsers) and rasterises it with ThorVG. */
#include "fuzz_common.h"

/* Harness resource bound, not a library contract: a header-declared canvas above this only measures the
 * allocator (libFuzzer's rss limit reports it as OOM), so such inputs are rejected with -1, not run. */
#define MAX_PIXELS (1024u * 1024u)

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

    lv_svg_node_t * doc = lv_svg_load_data((const char *)buf, (uint32_t)size);
    if(doc) lv_svg_node_delete(doc);

    lv_image_header_t header;
    if(lv_image_decoder_get_info(&img, &header) == LV_RESULT_OK) {
        if((uint64_t)header.w * header.h > MAX_PIXELS) {
            free(buf);
            return -1;
        }
        lv_image_decoder_dsc_t dsc;
        lv_image_decoder_args_t args;
        memset(&args, 0, sizeof(args));
        args.no_cache = true;
        if(lv_image_decoder_open(&dsc, &img, &args) == LV_RESULT_OK) {
            if(dsc.decoded) fuzz_touch(dsc.decoded->data, dsc.decoded->data_size);
            lv_image_decoder_close(&dsc);
        }
    }

    free(buf);
    return 0;
}
