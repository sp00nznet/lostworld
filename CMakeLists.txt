cmake_minimum_required(VERSION 3.20)
project(lostworld C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The board.
add_subdirectory(ext/model3recomp)

# The game. Generated from your own dump into src/recomp/ and never committed;
# see the README. Globbed because the file count depends on the romset.
file(GLOB LIFTED CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/*.c")
if(NOT LIFTED)
    message(FATAL_ERROR
        "src/recomp/ is empty. Recompile the game from your own dump first:\n"
        "  python tools/build_roms.py /path/to/lostwsga.zip roms/\n"
        "  python tools/lift.py\n"
        "See the README.")
endif()

add_executable(lostworld src/main.c ${LIFTED})

target_include_directories(lostworld PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp)

target_link_libraries(lostworld PRIVATE model3recomp)

if(MSVC)
    target_compile_options(lostworld PRIVATE /W3 /O2)
    target_compile_definitions(lostworld PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(lostworld PRIVATE -O2 -Wall -Wno-unused-variable)
endif()

# The browser build (emcmake cmake -S . -B build-web): lostworld.js + .wasm,
# booted by a page that supplies the ROM images and draws the frames. The
# guest runs on a pthread, so the page must be served cross-origin isolated.
if(EMSCRIPTEN)
    set_target_properties(lostworld PROPERTIES SUFFIX ".js")
    target_compile_options(lostworld PRIVATE -pthread)
    target_link_options(lostworld PRIVATE
        -O0 -sASSERTIONS=0 -pthread -sPROXY_TO_PTHREAD=1 -sPTHREAD_POOL_SIZE=20
        -sINITIAL_MEMORY=536870912 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2147483648
        -sSTACK_SIZE=16777216 -sDEFAULT_PTHREAD_STACK_SIZE=2097152
        -sMODULARIZE=1 -sEXPORT_NAME=createLostWorld -sENVIRONMENT=web,worker
        -sINVOKE_RUN=0 -sEXIT_RUNTIME=0
        -sEXPORTED_RUNTIME_METHODS=callMain,ENV,HEAPU8,HEAPU32,HEAP32,wasmMemory
        -sEXPORTED_FUNCTIONS=_main,_malloc,_free)
endif()
