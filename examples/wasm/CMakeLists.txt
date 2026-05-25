cmake_minimum_required(VERSION 3.12)
project(hasciicam_wasm_sample LANGUAGES C CXX)

add_executable(hasciicam_wasm_sample wasm_entry.c)
target_include_directories(hasciicam_wasm_sample PRIVATE ../../include)
target_link_libraries(hasciicam_wasm_sample PRIVATE hasciicam_core)

if(EMSCRIPTEN)
  set_target_properties(hasciicam_wasm_sample PROPERTIES
    OUTPUT_NAME "hasciicam"
    SUFFIX ".html"
  )
  target_link_options(hasciicam_wasm_sample PRIVATE
    "SHELL:-sEXPORTED_FUNCTIONS=['_hasciicam_wasm_init','_hasciicam_wasm_submit_rgba','_hasciicam_wasm_render','_hasciicam_wasm_ascii_text','_hasciicam_wasm_ascii_width','_hasciicam_wasm_ascii_height','_hasciicam_wasm_shutdown','_malloc','_free']"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['cwrap','getValue','UTF8ToString','HEAPU8']"
  )
endif()
