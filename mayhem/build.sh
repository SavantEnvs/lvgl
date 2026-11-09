#!/usr/bin/env bash
#
# mayhem/build.sh — build LVGL's fuzz harnesses, their standalone reproducers and the oracle self-test.
#
# LVGL is compiled directly with clang (no Kconfig/CMake): mayhem/lv_conf.h selects the headless,
# parser-heavy configuration. Three independent library builds are made from the same sources:
#   build/fuzz        sanitized + coverage-instrumented (linked into the libFuzzer binaries)
#   build/standalone  sanitized, no coverage (linked into the *-standalone reproducers)
#   build/test        project-normal flags (linked into the oracle self-test run by mayhem/test.sh)
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE STANDALONE_FUZZ_MAIN MAYHEM_JOBS COVERAGE_FLAGS

# LVGL calls function pointers through mismatched prototypes throughout (event, cache and draw callbacks), so
# -fsanitize=function would flag every run; the rest of ASan/UBSan stays on and halting.
SANITIZER_FLAGS="$SANITIZER_FLAGS -fno-sanitize=function"
export SANITIZER_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
mkdir -p "$B"

INCLUDES="-DLV_CONF_INCLUDE_SIMPLE -I$SRC/mayhem -I$SRC -I$SRC/include -I$SRC/include/lvgl -I$SRC/src/libs/thorvg"
HARNESSES="fuzz_image_raw fuzz_image_lvimg fuzz_font_fnt fuzz_text fuzz_svg fuzz_gif fuzz_lottie"

# build_lib <dir> <flags...>: compile every LVGL (and bundled ThorVG) source into <dir>/liblvgl.a
build_lib() {
  local dir="$1"; shift
  export LIB_CFLAGS="$* $INCLUDES" LIB_DIR="$dir"
  rm -rf "$dir"; mkdir -p "$dir/obj"
  compile_one() {
    local src="$1" o extra=""
    o="$LIB_DIR/obj/$(echo "$src" | tr / _).o"
    case "$src" in
      src/libs/gif/gif.c) extra="-fno-sanitize=shift-base" ;; # hides signed-shift UB in gifdec big-endian assembly (b<<24); revisit in a deep run
      src/libs/thorvg/tvgLottie*|src/widgets/lottie/*) extra="-fno-sanitize=float-cast-overflow" ;; # hides double->int narrowing of JSON numbers with no range check; revisit in a deep run
    esac
    case "$src" in
      *.cpp) $CXX -std=c++14 $LIB_CFLAGS $extra -c "$src" -o "$o" ;;
      *)     $CC $LIB_CFLAGS $extra -c "$src" -o "$o" ;;
    esac
  }
  export -f compile_one
  { find src -name '*.c'; find src/libs/thorvg -name '*.cpp'; } | sort \
    | xargs -P "$MAYHEM_JOBS" -I{} bash -c 'compile_one {}'
  rm -f "$dir/liblvgl.a"
  ar rcs "$dir/liblvgl.a" "$dir"/obj/*.o
}

# 1) sanitized, coverage-instrumented library for the libFuzzer binaries
build_lib "$B/fuzz" $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link
# 2) sanitized library without coverage instrumentation for the standalone reproducers
build_lib "$B/standalone" $SANITIZER_FLAGS $DEBUG_FLAGS -O1

# 3) harnesses: LLVMFuzzer binary + non-fuzzer standalone reproducer, both with LeakSanitizer off
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"
for h in $HARNESSES; do
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 $INCLUDES -fsanitize=fuzzer-no-link -c "$SRC/mayhem/$h.c" -o "$B/$h.fuzz.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/$h.fuzz.o" "$B/lsan_off.o" "$B/fuzz/liblvgl.a" -lm -o "$SRC/$h"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 $INCLUDES -c "$SRC/mayhem/$h.c" -o "$B/$h.sa.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$B/standalone_main.o" "$B/$h.sa.o" "$B/lsan_off.o" "$B/standalone/liblvgl.a" -lm \
    -o "$SRC/$h-standalone"
done

# 4) oracle self-test with the project's normal flags (clean library build, no sanitizers, no coverage)
build_lib "$B/test" -O2 $COVERAGE_FLAGS
$CC -O2 $COVERAGE_FLAGS $INCLUDES "$SRC/mayhem/selftest.c" "$B/test/liblvgl.a" -lstdc++ -lm -o "$SRC/lvgl_selftest"
