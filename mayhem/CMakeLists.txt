# Mayhem integration: builds axmol's vendored OpenAL-soft (3rdparty/openal) the way the engine does
# (3rdparty/CMakeLists.txt: static, axmol's fmt, no modules/utils/examples) but headless: only the
# loopback/null backends, so the fuzz harness renders audio without sound hardware.
cmake_minimum_required(VERSION 3.22)
project(axmol_mayhem C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(LIBTYPE STATIC CACHE STRING "" FORCE)
foreach(opt ALSOFT_ENABLE_MODULES ALSOFT_DLOPEN ALSOFT_UTILS ALSOFT_EXAMPLES ALSOFT_INSTALL
            ALSOFT_INSTALL_CONFIG ALSOFT_INSTALL_HRTF_DATA ALSOFT_INSTALL_AMBDEC_PRESETS
            ALSOFT_INSTALL_UTILS ALSOFT_INSTALL_EXAMPLES ALSOFT_UPDATE_BUILD_VERSION ALSOFT_RTKIT
            ALSOFT_BACKEND_PIPEWIRE ALSOFT_BACKEND_PULSEAUDIO ALSOFT_BACKEND_ALSA ALSOFT_BACKEND_OSS
            ALSOFT_BACKEND_SOLARIS ALSOFT_BACKEND_SNDIO ALSOFT_BACKEND_JACK ALSOFT_BACKEND_PORTAUDIO
            ALSOFT_BACKEND_SDL2 ALSOFT_BACKEND_SDL3 ALSOFT_BACKEND_WAVE)
  set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(AXSRC ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${AXSRC}/3rdparty/fmt fmt EXCLUDE_FROM_ALL)
add_subdirectory(${AXSRC}/3rdparty/openal openal EXCLUDE_FROM_ALL)
