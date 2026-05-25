cmake_minimum_required(VERSION 3.12)
project(hasciicam VERSION 2.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(ENABLE_ASAN "Enable AddressSanitizer for debugging" OFF)
option(HASCIICAM_BUILD_CLI "Build hasciicam CLI executable" ON)
option(HASCIICAM_ENABLE_TESTS "Enable hasciicam unit tests" ON)
option(HASCIICAM_ENABLE_SDL "Enable SDL output driver" ON)
option(HASCIICAM_ENABLE_X11 "Enable X11 output driver" ON)
option(HASCIICAM_ENABLE_CURSES "Enable curses output driver" ON)
option(HASCIICAM_ENABLE_CAPTURE_V4L2 "Enable V4L2 capture backend" ON)
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
    list(APPEND HASCIICAM_AALIB_SOURCES src/aalib/aasdl.c)
endif()

# Conditionally add X11 driver
if(HASCIICAM_WITH_X11)
    list(APPEND HASCIICAM_AALIB_SOURCES
        src/aalib/aax.c
        src/aalib/aaxkbd.c
        src/aalib/aaxmouse.c
    )
endif()

# Create aalib static library
add_library(aalib STATIC ${HASCIICAM_AALIB_SOURCES})
target_include_directories(aalib PUBLIC src/aalib)
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
)
target_include_directories(hasciicam_core PUBLIC include)
if(HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION)
  target_sources(hasciicam_core PRIVATE src/capture/capture_avfoundation.mm)
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

if(HASCIICAM_ENABLE_CAPTURE_MF OR HASCIICAM_ENABLE_CAPTURE_DSHOW)
  target_link_libraries(hasciicam_core PUBLIC mfplat mf mfreadwrite mfuuid ole32 uuid strmiids oleaut32)
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
message(STATUS "  HASCIICAM_ENABLE_SDL=${HASCIICAM_ENABLE_SDL} (found=${HASCIICAM_WITH_SDL})")
message(STATUS "  HASCIICAM_ENABLE_X11=${HASCIICAM_ENABLE_X11} (found=${HASCIICAM_WITH_X11})")
message(STATUS "  HASCIICAM_ENABLE_CURSES=${HASCIICAM_ENABLE_CURSES} (found=${HASCIICAM_WITH_CURSES})")
message(STATUS "  HASCIICAM_ENABLE_CAPTURE_V4L2=${HASCIICAM_ENABLE_CAPTURE_V4L2}")
message(STATUS "  HASCIICAM_ENABLE_CAPTURE_MF=${HASCIICAM_ENABLE_CAPTURE_MF}")
message(STATUS "  HASCIICAM_ENABLE_CAPTURE_DSHOW=${HASCIICAM_ENABLE_CAPTURE_DSHOW}")
message(STATUS "  HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION=${HASCIICAM_ENABLE_CAPTURE_AVFOUNDATION}")

# Install targets
if(HASCIICAM_BUILD_CLI)
  install(TARGETS hasciicam DESTINATION bin)
endif()
install(FILES doc/hasciicam.1 DESTINATION share/man/man1 OPTIONAL)

if(HASCIICAM_ENABLE_TESTS)
  enable_testing()
  add_executable(test_frame_convert
    tests/test_frame_convert.c
    src/capture/frame_convert.c
  )
  target_include_directories(test_frame_convert PRIVATE src/capture)
  add_test(NAME frame_convert COMMAND test_frame_convert)

  add_executable(test_core_link tests/test_core_link.c)
  target_include_directories(test_core_link PRIVATE include)
  target_link_libraries(test_core_link PRIVATE hasciicam_core)
  add_test(NAME core_link COMMAND test_core_link)
endif()

if(APPLE)
  add_executable(hasciicam_macos_host_sample examples/macos-host/main.c)
  target_include_directories(hasciicam_macos_host_sample PRIVATE include)
  target_link_libraries(hasciicam_macos_host_sample PRIVATE hasciicam_core)
endif()
