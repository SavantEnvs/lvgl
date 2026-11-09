/* UTF-8 text layout, Unicode bidi reordering and Arabic/Persian shaping on an arbitrary byte string. */
#include "fuzz_common.h"
#include "src/misc/lv_text_private.h"
#include "src/misc/lv_bidi_private.h"
#include "src/misc/lv_text_ap.h"

/* Harness input bound (layout and bidi cost grow with length), not a library contract. */
#define MAX_LEN 4096

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    fuzz_lvgl_init();
    if(size > MAX_LEN) return -1;

    char * txt = (char *)malloc(size + 1);
    memcpy(txt, data, size);
    txt[size] = '\0';
    size_t len = strlen(txt);

    const lv_font_t * font = &lv_font_montserrat_14;

    lv_point_t sz;
    lv_text_get_size(&sz, txt, font, 0, 0, 200, LV_TEXT_FLAG_NONE);
    lv_text_get_size(&sz, txt, font, 1, 2, 80, LV_TEXT_FLAG_BREAK_ALL | LV_TEXT_FLAG_RECOLOR);

    lv_text_attributes_t attr;
    lv_text_attributes_init(&attr);
    attr.max_width = 100;
    uint32_t pos = 0;
    while(pos < len) {
        int32_t used;
        uint32_t adv = lv_text_get_next_line(txt + pos, (uint32_t)(len - pos), font, &used, &attr);
        if(adv == 0) break;
        pos += adv;
    }

    char * out = (char *)malloc(len + 1);
    lv_bidi_process(txt, out, LV_BASE_DIR_LTR);
    lv_bidi_process(txt, out, LV_BASE_DIR_RTL);
    lv_bidi_detect_base_dir(txt);
    free(out);

    size_t ap_len = lv_text_ap_strlen(txt);
    char * ap = (char *)malloc(ap_len + 1);
    lv_text_ap_proc(txt, ap);
    free(ap);

    free(txt);
    return 0;
}
