cmake_minimum_required(VERSION 3.12)
project(hasciicam VERSION 2.0 LANGUAGES C CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(ENABLE_ASAN "Enable AddressSanitizer for debugging" OFF)
option(HASCIICAM_BUILD_CLI "Build hasciicam CLI executable" ON)
option(HASCIICAM_ENABLE_TESTS "Enable hasciicam unit tests" ON)
option(HASCIICAM_ENABLE_VISUAL_TESTS "Enable opt-in visual smoke tests" OFF)
option(HASCIICAM_ENABLE_CAMERA_TESTS "Enable opt-in real camera smoke tests" OFF)
set(HASCIICAM_TEST_CAMERA_DEVICE "" CACHE STRING "Camera device selector for opt-in camera smoke tests")
option(HASCIICAM_ENABLE_SDL "Enable SDL output driver" ON)
option(HASCIICAM_ENABLE_X11 "Enable X11 output driver" ON)
option(HASCIICAM_ENABLE_CURSES "Enable curses output driver" ON)
option(HASCIICAM_ENABLE_GUI "Enable on-screen Dear ImGui controls in SDL live mode" ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  option(HASCIICAM_ENABLE_CAPTURE_V4L2 "Enable V4L2 capture backend" ON)
else()
  option(HASCIICAM_ENABLE_CAPTURE_V4L2 "Enable V4L2 capture backend" OFF)
endif()
if(WIN32)
  option(HASCIICAM_ENABLE_CAPTURE_MF "Enable Media Foundation capture backend" ON)
  option(HASCIICAM_ENABLE_CAPTURE_DSHOW "Enable DirectShow capture backend fallback" ON)
else()
  option(HASCIICAM_ENABLE_CAPTURE_MF "Enable Media Foundation capture backend" OFF)
  option(HASCIICAM_ENABLE_CAPTURE_DSHOW "Enable DirectShow capture backend fallback" OFF)
endif()
if(APPLE)
  option(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION "Enable AVFoundation capture backend" ON)
else()
  option(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION "Enable AVFoundation capture backend" OFF)
endif()

set(HASCIICAM_WITH_SDL OFF)
set(HASCIICAM_WITH_X11 OFF)
set(HASCIICAM_WITH_CURSES OFF)

if(HASCIICAM_ENABLE_SDL)
  find_package(SDL2 QUIET)
  if(TARGET SDL2::SDL2)
    set(HASCIICAM_WITH_SDL ON)
    add_compile_definitions(SDL_DRIVER)
  else()
    message(FATAL_ERROR
      "HASCIICAM_ENABLE_SDL is ON, but CMake could not find SDL2. "
      "Install SDL2 or pass -DHASCIICAM_ENABLE_SDL=OFF. "
      "On Windows with vcpkg, configure with "
      "-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake.")
  endif()
endif()

if(HASCIICAM_ENABLE_X11 AND UNIX AND NOT APPLE)
  find_package(X11 QUIET)
  if(X11_FOUND)
    set(HASCIICAM_WITH_X11 ON)
    add_compile_definitions(X11_DRIVER)
  endif()
endif()

if(HASCIICAM_ENABLE_CURSES)
  find_package(Curses QUIET)
  if(Curses_FOUND)
    set(HASCIICAM_WITH_CURSES ON)
    add_compile_definitions(CURSES_DRIVER)
  endif()
endif()

# ASAN flags
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

include(cmake/HasciiCamSources.cmake)

# Conditionally add SDL driver
if(HASCIICAM_WITH_SDL)
    list(APPEND HASCIICAM_AALIB_SOURCES third_party/aalib/aasdl.c)
endif()

# Conditionally add X11 driver
if(HASCIICAM_WITH_X11)
    list(APPEND HASCIICAM_AALIB_SOURCES
        third_party/aalib/aax.c
        third_party/aalib/aaxkbd.c
        third_party/aalib/aaxmouse.c
    )
endif()

# Create aalib static library
add_library(aalib STATIC ${HASCIICAM_AALIB_SOURCES})
target_include_directories(aalib PUBLIC third_party/aalib)
target_compile_definitions(aalib PRIVATE HASCIICAM_APP_TITLE="hasciicam ${PROJECT_VERSION}")
if(WIN32)
    target_include_directories(aalib PUBLIC src/compat)
    target_compile_definitions(aalib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Link libraries to aalib
if(HASCIICAM_WITH_SDL)
    target_include_directories(aalib PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

if(HASCIICAM_WITH_X11)
    target_include_directories(aalib PRIVATE ${X11_INCLUDE_DIR})
endif()

if(HASCIICAM_WITH_CURSES)
    target_include_directories(aalib PRIVATE ${CURSES_INCLUDE_DIR})
endif()

set(HASCIICAM_CAPTURE_DEFINITIONS)
if(HASCIICAM_ENABLE_CAPTURE_V4L2)
  list(APPEND HASCIICAM_CAPTURE_DEFINITIONS HASCIICAM_ENABLE_CAPTURE_V4L2=1)
else()
  list(REMOVE_ITEM HASCIICAM_CAPTURE_SOURCES src/capture/capture_v4l2.c)
endif()

if(HASCIICAM_ENABLE_CAPTURE_MF)
  list(APPEND HASCIICAM_CAPTURE_DEFINITIONS HASCIICAM_ENABLE_CAPTURE_MF=1)
else()
  list(REMOVE_ITEM HASCIICAM_CAPTURE_SOURCES src/capture/capture_mf.c)
endif()

if(HASCIICAM_ENABLE_CAPTURE_DSHOW)
  list(APPEND HASCIICAM_CAPTURE_DEFINITIONS HASCIICAM_ENABLE_CAPTURE_DSHOW=1)
else()
  list(REMOVE_ITEM HASCIICAM_CAPTURE_SOURCES src/capture/capture_dshow.cpp)
endif()

if(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION)
  list(APPEND HASCIICAM_CAPTURE_DEFINITIONS HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION=1)
endif()

add_library(hasciicam_core STATIC
    ${HASCIICAM_APP_SOURCES}
    ${HASCIICAM_OUTPUT_SOURCES}
    ${HASCIICAM_RENDER_SOURCES}
    ${HASCIICAM_CAPTURE_SOURCES}
    src/app/tomlc17/tomlc17.c
)
if(WIN32)
  target_sources(hasciicam_core PRIVATE src/gui/gui_file_dialog_win32.c)
else()
  target_sources(hasciicam_core PRIVATE src/gui/gui_file_dialog_fallback.c)
endif()
target_include_directories(hasciicam_core PUBLIC include)
target_include_directories(hasciicam_core PRIVATE src/app)
if(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION)
  target_sources(hasciicam_core PRIVATE src/capture/capture_avfoundation.mm)
endif()

set(HASCIICAM_WITH_GUI OFF)
set(HASCIICAM_GUI_SOURCES)
set(HASCIICAM_IMGUI_SOURCES)
if(HASCIICAM_ENABLE_GUI AND HASCIICAM_WITH_SDL)
  if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.h")
    set(HASCIICAM_WITH_GUI ON)
    list(APPEND HASCIICAM_IMGUI_SOURCES
      third_party/imgui/imgui.cpp
      third_party/imgui/imgui_draw.cpp
      third_party/imgui/imgui_tables.cpp
      third_party/imgui/imgui_widgets.cpp
      third_party/imgui/backends/imgui_impl_sdl2.cpp
      third_party/imgui/backends/imgui_impl_sdlrenderer2.cpp
    )
    list(APPEND HASCIICAM_GUI_SOURCES src/gui/gui_overlay.cpp)
    target_sources(aalib PRIVATE ${HASCIICAM_GUI_SOURCES} ${HASCIICAM_IMGUI_SOURCES})
    target_include_directories(aalib PRIVATE
      third_party/imgui
      third_party/imgui/backends
      src/gui
      src/virtual_camera
    )
    target_compile_definitions(aalib PRIVATE HASCIICAM_ENABLE_GUI=1)
  else()
    message(STATUS "HASCIICAM_ENABLE_GUI requested but third_party/imgui not found; GUI disabled")
  endif()
endif()
target_compile_definitions(hasciicam_core PRIVATE ${HASCIICAM_CAPTURE_DEFINITIONS})
target_link_libraries(hasciicam_core PUBLIC aalib)
if(WIN32)
  target_include_directories(hasciicam_core PRIVATE src/compat)
  target_compile_definitions(hasciicam_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Additional system libraries that may be needed
if(NOT MSVC)
    target_link_libraries(hasciicam_core PUBLIC m)
endif()

# Link optional libraries to hasciicam core
if(HASCIICAM_WITH_SDL)
  target_link_libraries(hasciicam_core PUBLIC SDL2::SDL2 SDL2::SDL2main)
endif()

if(HASCIICAM_WITH_X11)
  target_link_libraries(hasciicam_core PUBLIC X11::X11)
endif()

if(HASCIICAM_WITH_CURSES)
  target_link_libraries(hasciicam_core PUBLIC ${CURSES_LIBRARIES})
  target_include_directories(hasciicam_core PUBLIC ${CURSES_INCLUDE_DIRS})
endif()

if(WIN32)
  target_link_libraries(hasciicam_core PUBLIC mfplat mf mfreadwrite mfuuid ole32 uuid strmiids oleaut32 mfsensorgroup)
endif()
if(HASCIICAM_WITH_GUI AND WIN32)
  target_link_libraries(hasciicam_core PUBLIC comdlg32)
endif()

if(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION)
  target_link_libraries(hasciicam_core PUBLIC
    "-framework AVFoundation"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework Foundation"
  )
endif()

if(HASCIICAM_BUILD_CLI)
  # CLI host executable
  add_executable(hasciicam src/hasciicam.c)
  target_compile_definitions(hasciicam PRIVATE
      PACKAGE="hasciicam"
      VERSION="${PROJECT_VERSION}"
  )
  if(WIN32)
      target_include_directories(hasciicam PRIVATE src/compat)
      target_compile_definitions(hasciicam PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  target_link_libraries(hasciicam PRIVATE hasciicam_core)
  if(WIN32 AND HASCIICAM_WITH_SDL)
    add_custom_command(TARGET hasciicam POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:hasciicam>
      $<TARGET_FILE_DIR:hasciicam>
      COMMAND_EXPAND_LISTS
    )
  endif()

endif()

message(STATUS "hasciicam options:")
message(STATUS "  HASCIICAM_BUILD_CLI=${HASCIICAM_BUILD_CLI}")
message(STATUS "  HASCIICAM_ENABLE_TESTS=${HASCIICAM_ENABLE_TESTS}")
message(STATUS "  HASCIICAM_ENABLE_VISUAL_TESTS=${HASCIICAM_ENABLE_VISUAL_TESTS}")
message(STATUS "  HASCIICAM_ENABLE_CAMERA_TESTS=${HASCIICAM_ENABLE_CAMERA_TESTS}")
message(STATUS "  HASCIICAM_TEST_CAMERA_DEVICE=${HASCIICAM_TEST_CAMERA_DEVICE}")
message(STATUS "  HASCIICAM_ENABLE_SDL=${HASCIICAM_ENABLE_SDL} (found=${HASCIICAM_WITH_SDL})")
message(STATUS "  HASCIICAM_ENABLE_X11=${HASCIICAM_ENABLE_X11} (found=${HASCIICAM_WITH_X11})")
message(STATUS "  HASCIICAM_ENABLE_CURSES=${HASCIICAM_ENABLE_CURSES} (found=${HASCIICAM_WITH_CURSES})")
message(STATUS "  HASCIICAM_ENABLE_GUI=${HASCIICAM_ENABLE_GUI} (enabled=${HASCIICAM_WITH_GUI})")
message(STATUS "  HASCIICAM_ENABLE_CAPTURE_V4L2=${HASCIICAM_ENABLE_CAPTURE_V4L2}")
message(STATUS "  HASCIICAM_ENABLE_CAPTURE_MF=${HASCIICAM_ENABLE_CAPTURE_MF}")
message(STATUS "  HASCIICAM_ENABLE_CAPTURE_DSHOW=${HASCIICAM_ENABLE_CAPTURE_DSHOW}")
message(STATUS "  HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION=${HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION}")

# Install targets
if(HASCIICAM_BUILD_CLI)
  install(TARGETS hasciicam DESTINATION bin)
endif()
install(FILES docs/hasciicam.1 DESTINATION share/man/man1 OPTIONAL)

if(HASCIICAM_ENABLE_TESTS)
  enable_testing()
  add_executable(test_frame_convert
    tests/test_frame_convert.c
    src/capture/frame_convert.c
  )
  target_include_directories(test_frame_convert PRIVATE src/capture)
  add_test(NAME frame_convert COMMAND test_frame_convert)
  set_tests_properties(frame_convert PROPERTIES LABELS "unit;core")

  add_executable(test_app_size
    tests/test_app_size.c
    src/app/app_size.c
  )
  target_include_directories(test_app_size PRIVATE src/app)
  add_test(NAME app_size COMMAND test_app_size)
  set_tests_properties(app_size PROPERTIES LABELS "unit;core")

  add_executable(test_render_font
    tests/test_render_font.c
  )
  target_include_directories(test_render_font PRIVATE src/render third_party/aalib)
  target_link_libraries(test_render_font PRIVATE aalib)
  if(WIN32)
    target_compile_definitions(test_render_font PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  add_test(NAME render_font COMMAND test_render_font)
  set_tests_properties(render_font PROPERTIES LABELS "unit;core")

  add_executable(test_app_config
    tests/test_app_config.c
    src/app/app_config.c
    src/app/tomlc17/tomlc17.c
    src/virtual_camera/virtual_camera.c
  )
  target_include_directories(test_app_config PRIVATE src/app src/render third_party/aalib)
  target_link_libraries(test_app_config PRIVATE aalib)
  if(WIN32)
    target_sources(test_app_config PRIVATE src/compat/getopt.c)
    target_include_directories(test_app_config PRIVATE src/compat)
    target_compile_definitions(test_app_config PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  add_test(NAME app_config COMMAND test_app_config)
  set_tests_properties(app_config PROPERTIES LABELS "unit;core")

  add_executable(test_app_virtual_camera
    tests/test_app_virtual_camera.c
    tests/test_app_virtual_camera_windows_stub.c
    src/app/app_virtual_camera.c
    src/app/app_config.c
    src/app/tomlc17/tomlc17.c
    src/virtual_camera/virtual_camera.c
  )
  target_include_directories(test_app_virtual_camera PRIVATE src/app src/virtual_camera src/gui)
  target_link_libraries(test_app_virtual_camera PRIVATE aalib)
  if(WIN32)
    target_sources(test_app_virtual_camera PRIVATE src/virtual_camera/windows/pipe/hasciicam_virtual_camera_pipe.c)
    target_sources(test_app_virtual_camera PRIVATE src/virtual_camera/virtual_camera_convert.c)
    target_compile_definitions(test_app_virtual_camera PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  add_test(NAME app_virtual_camera COMMAND test_app_virtual_camera)
  set_tests_properties(app_virtual_camera PROPERTIES LABELS "unit;core")

  add_executable(test_gui_state
    tests/test_gui_state.c
    src/gui/gui_state.c
  )
  target_include_directories(test_gui_state PRIVATE src/gui src/app src/capture)
  add_test(NAME gui_state COMMAND test_gui_state)
  set_tests_properties(gui_state PROPERTIES LABELS "unit;core")

  add_executable(test_capture_size
    tests/test_capture_size.c
    src/capture/capture_size.c
  )
  target_include_directories(test_capture_size PRIVATE src/capture)
  add_test(NAME capture_size COMMAND test_capture_size)
  set_tests_properties(capture_size PROPERTIES LABELS "unit;core")

  add_executable(test_virtual_camera
    tests/test_virtual_camera.c
    src/virtual_camera/virtual_camera.c
    src/virtual_camera/virtual_camera_convert.c
  )
  target_include_directories(test_virtual_camera PRIVATE src/virtual_camera)
  if(WIN32)
    target_sources(test_virtual_camera PRIVATE src/virtual_camera/windows/pipe/hasciicam_virtual_camera_pipe.c)
  endif()
  add_test(NAME virtual_camera COMMAND test_virtual_camera)
  set_tests_properties(virtual_camera PROPERTIES LABELS "unit;core")

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(test_virtual_camera_v4l2
      tests/test_virtual_camera_v4l2.c
      src/virtual_camera/virtual_camera.c
    )
    target_include_directories(test_virtual_camera_v4l2 PRIVATE src/virtual_camera)
    add_test(NAME virtual_camera_v4l2 COMMAND test_virtual_camera_v4l2)
    set_tests_properties(virtual_camera_v4l2 PROPERTIES LABELS "unit;core")
  endif()

  if(WIN32)
    add_library(hasciicam_virtual_camera_source SHARED
      src/virtual_camera/windows/pipe/hasciicam_virtual_camera_pipe.c
      src/virtual_camera/windows/source/hasciicam_virtual_camera_source.cpp
      src/virtual_camera/virtual_camera.c
      src/virtual_camera/virtual_camera_convert.c
    )
    target_include_directories(hasciicam_virtual_camera_source PRIVATE
      src/virtual_camera/windows/install
      src/virtual_camera/windows/pipe
      src/virtual_camera/windows/source
      src/virtual_camera)
    target_compile_definitions(hasciicam_virtual_camera_source PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(hasciicam_virtual_camera_source PRIVATE
      mfplat mf mfreadwrite mfuuid ole32 uuid strmiids oleaut32)

    add_executable(hasciicam_vcamctl
      src/virtual_camera/windows/tool/hasciicam_vcamctl.c
      src/virtual_camera/windows/install/hasciicam_virtual_camera_install.c
    )
    target_include_directories(hasciicam_vcamctl PRIVATE
      src/virtual_camera/windows/install)
    target_compile_definitions(hasciicam_vcamctl PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME hasciicam_vcamctl_help COMMAND hasciicam_vcamctl --help)
    set_tests_properties(hasciicam_vcamctl_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: hasciicam-vcam")
    set_tests_properties(hasciicam_vcamctl_help PROPERTIES LABELS "cli")

    add_executable(test_app_virtual_camera_windows
      tests/test_app_virtual_camera_windows.c
      src/app/app_virtual_camera_windows.c
    )
    target_include_directories(test_app_virtual_camera_windows PRIVATE
      src/app
      src/virtual_camera/windows/source
      src/virtual_camera)
    target_compile_definitions(test_app_virtual_camera_windows PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(test_app_virtual_camera_windows PRIVATE
      aalib mfplat mf mfreadwrite mfuuid ole32 uuid strmiids oleaut32)
    add_test(NAME app_virtual_camera_windows COMMAND test_app_virtual_camera_windows)
    set_tests_properties(app_virtual_camera_windows PROPERTIES LABELS "unit;core")

    add_executable(test_virtual_camera_windows
      tests/test_virtual_camera_windows.c
      src/virtual_camera/windows/pipe/hasciicam_virtual_camera_pipe.c
      src/virtual_camera/windows/source/hasciicam_virtual_camera_source.cpp
      src/virtual_camera/virtual_camera.c
      src/virtual_camera/virtual_camera_convert.c
    )
    target_include_directories(test_virtual_camera_windows PRIVATE
      src/virtual_camera/windows/pipe
      src/virtual_camera/windows/source
      src/virtual_camera)
    target_compile_definitions(test_virtual_camera_windows PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(test_virtual_camera_windows PRIVATE
      mfplat mf mfreadwrite mfuuid ole32 uuid strmiids oleaut32)
    add_test(NAME virtual_camera_windows COMMAND test_virtual_camera_windows)
    set_tests_properties(virtual_camera_windows PROPERTIES LABELS "unit;core")

    add_executable(test_virtual_camera_install
      tests/test_virtual_camera_install.c
      src/virtual_camera/windows/install/hasciicam_virtual_camera_install.c
    )
    target_include_directories(test_virtual_camera_install PRIVATE
      src/virtual_camera/windows/install)
    target_compile_definitions(test_virtual_camera_install PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME virtual_camera_install COMMAND test_virtual_camera_install)
    set_tests_properties(virtual_camera_install PROPERTIES LABELS "unit;core")

    install(TARGETS hasciicam_virtual_camera_source hasciicam_vcamctl
            RUNTIME DESTINATION bin)
  endif()

  add_executable(test_capture_control
    tests/test_capture_control.c
    src/capture/capture_control.c
  )
  target_include_directories(test_capture_control PRIVATE src/capture)
  add_test(NAME capture_control COMMAND test_capture_control)
  set_tests_properties(capture_control PROPERTIES LABELS "unit;core")

  add_executable(test_core_link tests/test_core_link.c)
  target_include_directories(test_core_link PRIVATE include)
  target_link_libraries(test_core_link PRIVATE hasciicam_core)
  add_test(NAME core_link COMMAND test_core_link)
  set_tests_properties(core_link PROPERTIES LABELS "unit;core")

  add_executable(test_pipeline_smoke tests/test_pipeline_smoke.c)
  target_include_directories(test_pipeline_smoke PRIVATE include)
  target_link_libraries(test_pipeline_smoke PRIVATE hasciicam_core)
  add_test(NAME pipeline_smoke COMMAND test_pipeline_smoke)
  set_tests_properties(pipeline_smoke PROPERTIES LABELS "unit;core")

  add_executable(test_capture_synthetic tests/test_capture_synthetic.c src/capture/capture_backend.c src/capture/capture_external.c src/capture/capture_synthetic.c)
  target_include_directories(test_capture_synthetic PRIVATE src/capture)
  add_test(NAME capture_synthetic COMMAND test_capture_synthetic)
  set_tests_properties(capture_synthetic PROPERTIES LABELS "unit;core")

  if(HASCIICAM_BUILD_CLI)
    add_test(NAME cli_help COMMAND hasciicam -h)
    set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: hasciicam")
    set_tests_properties(cli_help PROPERTIES LABELS "cli")

    add_test(NAME cli_aahelp COMMAND hasciicam -H)
    set_tests_properties(cli_aahelp PROPERTIES PASS_REGULAR_EXPRESSION "aalib options")
    set_tests_properties(cli_aahelp PROPERTIES LABELS "cli")

    add_test(
      NAME cli_stdout
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_MODE=stdout
        -DHASCIICAM_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-stdout-smoke.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke.cmake
    )
    set_tests_properties(cli_stdout PROPERTIES LABELS "cli")

    add_test(
      NAME cli_text
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_MODE=text
        -DHASCIICAM_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-text-smoke.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke.cmake
    )
    set_tests_properties(cli_text PROPERTIES LABELS "cli")

    add_test(
      NAME cli_html
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_MODE=html
        -DHASCIICAM_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-html-smoke.html
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke.cmake
    )
    set_tests_properties(cli_html PROPERTIES LABELS "cli")

    add_test(
      NAME cli_render_tuning
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_render_tuning.cmake
    )
    set_tests_properties(cli_render_tuning PROPERTIES LABELS "cli")

    add_test(
      NAME cli_config_startup
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_config_startup.cmake
    )
    set_tests_properties(cli_config_startup PROPERTIES LABELS "cli")

    add_test(
      NAME cli_font_options
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_font_options.cmake
    )
    set_tests_properties(cli_font_options PROPERTIES LABELS "cli")

    add_test(
      NAME cli_size_pixel
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_CASE=pixel
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_size_semantics.cmake
    )
    set_tests_properties(cli_size_pixel PROPERTIES LABELS "cli")

    add_test(
      NAME cli_size_chars
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_CASE=chars
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_size_semantics.cmake
    )
    set_tests_properties(cli_size_chars PROPERTIES LABELS "cli")

    add_test(
      NAME cli_size_html_short_s
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_CASE=html_short_s
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_size_semantics.cmake
    )
    set_tests_properties(cli_size_html_short_s PROPERTIES LABELS "cli")

    add_test(
      NAME cli_size_html_pixel_reject
      COMMAND ${CMAKE_COMMAND}
        -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
        -DHASCIICAM_CASE=html_pixel_reject
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_size_semantics.cmake
    )
    set_tests_properties(cli_size_html_pixel_reject PROPERTIES LABELS "cli")

    if(HASCIICAM_ENABLE_VISUAL_TESTS)
      if(NOT HASCIICAM_WITH_SDL)
        message(FATAL_ERROR "HASCIICAM_ENABLE_VISUAL_TESTS requires SDL support.")
      endif()
      add_test(
        NAME visual_sdl
        COMMAND ${CMAKE_COMMAND}
          -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
          -DHASCIICAM_MODE=sdl
          -DHASCIICAM_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/visual-sdl.log
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke.cmake
      )
      set_tests_properties(visual_sdl PROPERTIES LABELS "visual")
    endif()

    if(HASCIICAM_ENABLE_CAMERA_TESTS)
      if(HASCIICAM_TEST_CAMERA_DEVICE STREQUAL "")
        message(WARNING "HASCIICAM_ENABLE_CAMERA_TESTS is ON but HASCIICAM_TEST_CAMERA_DEVICE is empty; camera tests are not added.")
      else()
        add_test(
          NAME camera_text
          COMMAND ${CMAKE_COMMAND}
            -DHASCIICAM_EXE=$<TARGET_FILE:hasciicam>
            -DHASCIICAM_MODE=camera_text
            -DHASCIICAM_DEVICE=${HASCIICAM_TEST_CAMERA_DEVICE}
            -DHASCIICAM_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/camera-text-smoke.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke.cmake
        )
        set_tests_properties(camera_text PROPERTIES LABELS "camera")
      endif()
    endif()
  endif()
endif()

if(APPLE)
  add_executable(hasciicam_macos_host_sample examples/macos-host/main.c)
  target_include_directories(hasciicam_macos_host_sample PRIVATE include)
  target_link_libraries(hasciicam_macos_host_sample PRIVATE hasciicam_core)
endif()

if(EMSCRIPTEN)
  add_subdirectory(examples/wasm)
endif()
