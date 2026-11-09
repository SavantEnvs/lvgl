/* LVGL configuration used by the Mayhem fuzz build: headless, every data-parsing decoder enabled. */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB   /* plain malloc so ASan sees every heap overflow */
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#define LV_USE_FLOAT            1
#define LV_USE_MATRIX           1
#define LV_USE_VECTOR_GRAPHIC   1
#define LV_USE_THORVG           1
#define LV_USE_THORVG_INTERNAL  1
#define LV_USE_SVG              1
#define LV_USE_LOTTIE           1

#define LV_USE_RLE              1
#define LV_BIN_DECODER_RAM_LOAD 1
#define LV_USE_LZ4              1
#define LV_USE_LZ4_INTERNAL     1
#define LV_USE_LODEPNG          1
#define LV_USE_BMP              1
#define LV_USE_TJPGD            1
#define LV_USE_GIF              1
#define LV_USE_QRCODE           1
#define LV_USE_BARCODE          1
#define LV_USE_BIDI             1
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_USE_FONT_COMPRESSED  1
#define LV_USE_TINY_TTF         1
#define LV_USE_FS_MEMFS         1
#define LV_FS_MEMFS_LETTER      'M'

#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_ASSERT_HANDLER       __builtin_trap();

#endif /*LV_CONF_H*/
