cmake_minimum_required(VERSION 3.12)
project(hasciicam_wasm_sample LANGUAGES C CXX)

add_executable(hasciicam_wasm_sample wasm_entry.c)
target_include_directories(hasciicam_wasm_sample PRIVATE ../../include)
target_link_libraries(hasciicam_wasm_sample PRIVATE hasciicam_core)

if(EMSCRIPTEN)
  set_target_properties(hasciicam_wasm_sample PROPERTIES
    OUTPUT_NAME "hasciicam"
    SUFFIX ".js"
  )
  target_link_options(hasciicam_wasm_sample PRIVATE
    "SHELL:-sEXPORTED_FUNCTIONS=['_hasciicam_wasm_init','_hasciicam_wasm_submit_rgba','_hasciicam_wasm_render','_hasciicam_wasm_ascii_text','_hasciicam_wasm_ascii_width','_hasciicam_wasm_ascii_height','_hasciicam_wasm_shutdown','_malloc','_free']"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['cwrap','getValue','UTF8ToString','HEAPU8']"
  )
  set(HASCIICAM_WASM_ASSET_OUTPUTS
    "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    "${CMAKE_CURRENT_BINARY_DIR}/main.js"
    "${CMAKE_CURRENT_BINARY_DIR}/dyne-mark.svg"
    "${CMAKE_CURRENT_BINARY_DIR}/dyne-logotype.svg"
  )
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
      "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/main.js"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/main.js"
      "${CMAKE_CURRENT_BINARY_DIR}/main.js"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/main.js"
    VERBATIM
  )
  add_custom_command(
    OUTPUT
      "${CMAKE_CURRENT_BINARY_DIR}/dyne-mark.svg"
      "${CMAKE_CURRENT_BINARY_DIR}/dyne-logotype.svg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/public/dyne-mark.svg"
      "${CMAKE_CURRENT_BINARY_DIR}/dyne-mark.svg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/public/dyne-logotype.svg"
      "${CMAKE_CURRENT_BINARY_DIR}/dyne-logotype.svg"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/public/dyne-mark.svg"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/public/dyne-logotype.svg"
    VERBATIM
  )
  add_custom_target(hasciicam_wasm_assets DEPENDS ${HASCIICAM_WASM_ASSET_OUTPUTS})
  add_dependencies(hasciicam_wasm_sample hasciicam_wasm_assets)
endif()
