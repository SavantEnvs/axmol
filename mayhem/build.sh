#!/usr/bin/env bash
# mayhem/build.sh — builds axmol's fuzz harnesses (sanitized) and the functional self-test (normal flags).
# Air-gapped and re-runnable: everything comes from the checkout (3rdparty/ is vendored); build dirs are
# under mayhem/build-* (gitignored by mayhem/.gitignore).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export CC CXX
cd "$SRC"
M="$SRC/mayhem"
INC=(-I"$SRC/3rdparty/openal/include")
# axmol core sources the Uri/TGA/SAX targets compile directly. The full engine library needs a GL/RHI
# stack, so only the translation units these entry points live in are built; they are compiled with
# -ffunction-sections and linked with lld --gc-sections so their unused file-path helpers (tgaLoad,
# SAXParser::parse(filename), which call FileUtils) are dropped instead of pulling in the whole engine.
AXINC=(-I"$SRC" -I"$SRC/axmol" -I"$SRC/3rdparty" -I"$SRC/3rdparty/yasio" -I"$SRC/3rdparty/fmt/include"
       -I"$SRC/3rdparty/robin-map/include")
AXSRCS=(axmol/network/Uri.cpp axmol/base/TGAlib.cpp axmol/platform/SAXParser.cpp axmol/base/Logging.cpp
        axmol/base/Data.cpp)
GC=(-ffunction-sections -fdata-sections)
GCLINK=(-fuse-ld=lld -Wl,--gc-sections)

# 1) Sanitized + instrumented OpenAL-soft (axmol's vendored copy), static, headless backends only.
# UBSan's signed-integer-overflow check is relaxed for the OpenAL library only: its MS-ADPCM decoder
# (core/voice.cpp:447, `delta * adaptation`) overflows int on almost every MS-ADPCM input with large
# nibbles, which halts every run before the block/span logic (the target of upstream fix 41ef0ba2) is
# reached. ASan and every other UBSan check stay on and halting.
FUZZ_CFLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
[ -n "$SANITIZER_FLAGS" ] && FUZZ_CFLAGS="$FUZZ_CFLAGS -fno-sanitize=signed-integer-overflow"
cmake -S "$M" -B "$M/build-fuzz" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="$FUZZ_CFLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_CFLAGS" \
  -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O1" -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O1"
cmake --build "$M/build-fuzz" -j"$MAYHEM_JOBS" --target OpenAL fmt
mapfile -t FUZZ_AL < <(for f in "$M"/build-fuzz/openal/libopenal.a "$M"/build-fuzz/openal/libalsoft.*.a "$M"/build-fuzz/fmt/libfmt*.a; do [ -f "$f" ] && echo "$f"; done)

# 2) Harnesses: libFuzzer binary + standalone reproducer each, into mayhem/out/.
mkdir -p "$M/out"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$M/lsan_off.cc" -o "$M/build-fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$M/build-fuzz/standalone_main.o"
for h in fuzz_openal_buffer; do
  $CXX -std=c++20 $SANITIZER_FLAGS $DEBUG_FLAGS -DAL_LIBTYPE_STATIC=1 "${INC[@]}" $LIB_FUZZING_ENGINE \
    "$M/harnesses/$h.cpp" "$M/build-fuzz/lsan_off.o" -Wl,--start-group "${FUZZ_AL[@]}" -Wl,--end-group \
    -lpthread -ldl -lm -o "$M/out/$h"
  $CXX -std=c++20 $SANITIZER_FLAGS $DEBUG_FLAGS -DAL_LIBTYPE_STATIC=1 "${INC[@]}" \
    "$M/harnesses/$h.cpp" "$M/build-fuzz/standalone_main.o" "$M/build-fuzz/lsan_off.o" \
    -Wl,--start-group "${FUZZ_AL[@]}" -Wl,--end-group -lpthread -ldl -lm -o "$M/out/$h-standalone"
done

# Core targets: axmol core sources, sanitized, linked against the fuzz build's fmt.
FUZZ_FMT="$(ls "$M"/build-fuzz/fmt/libfmt*.a | head -n1)"
AXOBJ=()
for f in "${AXSRCS[@]}"; do
  o="$M/build-fuzz/ax_$(basename "$f" .cpp).o"
  $CXX -std=c++20 -O1 "${GC[@]}" $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS "${AXINC[@]}" \
    -c "$SRC/$f" -o "$o"
  AXOBJ+=("$o")
done
for h in fuzz_uri_parse fuzz_tga_load fuzz_sax_parse; do
  $CXX -std=c++20 "${GC[@]}" $SANITIZER_FLAGS $DEBUG_FLAGS "${AXINC[@]}" $LIB_FUZZING_ENGINE "$M/harnesses/$h.cpp" \
    "${AXOBJ[@]}" "$M/build-fuzz/lsan_off.o" "$FUZZ_FMT" "${GCLINK[@]}" -o "$M/out/$h"
  $CXX -std=c++20 "${GC[@]}" $SANITIZER_FLAGS $DEBUG_FLAGS "${AXINC[@]}" "$M/harnesses/$h.cpp" \
    "${AXOBJ[@]}" "$M/build-fuzz/standalone_main.o" "$M/build-fuzz/lsan_off.o" "$FUZZ_FMT" "${GCLINK[@]}" \
    -o "$M/out/$h-standalone"
done

# 3) Functional self-test with the project's NORMAL flags (independent build tree).
env -u CFLAGS -u CXXFLAGS cmake -S "$M" -B "$M/build-test" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build "$M/build-test" -j"$MAYHEM_JOBS" --target OpenAL fmt
mapfile -t TEST_AL < <(for f in "$M"/build-test/openal/libopenal.a "$M"/build-test/openal/libalsoft.*.a "$M"/build-test/fmt/libfmt*.a; do [ -f "$f" ] && echo "$f"; done)
$CXX -std=c++20 -O1 $COVERAGE_FLAGS -DAL_LIBTYPE_STATIC=1 "${INC[@]}" "$M/harnesses/openal_selftest.cpp" \
  -Wl,--start-group "${TEST_AL[@]}" -Wl,--end-group -lpthread -ldl -lm -o "$M/build-test/openal_selftest"
TEST_FMT="$(ls "$M"/build-test/fmt/libfmt*.a | head -n1)"
for t in uri_selftest core_selftest; do
  $CXX -std=c++20 -O1 "${GC[@]}" $COVERAGE_FLAGS "${AXINC[@]}" "$M/harnesses/$t.cpp" \
    "${AXSRCS[@]/#/$SRC/}" "$TEST_FMT" "${GCLINK[@]}" -o "$M/build-test/$t"
done
