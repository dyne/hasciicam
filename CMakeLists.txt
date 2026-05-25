cmake_minimum_required(VERSION 3.12)
project(hasciicam VERSION 2.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(ENABLE_ASAN "Enable AddressSanitizer for debugging" OFF)
option(HASCIICAM_ENABLE_TESTS "Enable hasciicam unit tests" ON)

find_package(SDL2 QUIET)
if(TARGET SDL2::SDL2)
  message(STATUS "Found SDL2: ${SDL2_VERSION}")
  add_compile_definitions(SDL_DRIVER)
endif()

if(UNIX AND NOT APPLE)
  find_package(X11 QUIET)
  if(X11_FOUND)
    message(STATUS "Found X11")
    add_compile_definitions(X11_DRIVER)
  endif()
endif()

find_package(Curses QUIET)
if(Curses_FOUND)
    message(STATUS "Found Curses")
    add_compile_definitions(CURSES_DRIVER)
endif()

# ASAN flags
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

include(cmake/HasciiCamSources.cmake)

# Conditionally add SDL driver
if(TARGET SDL2::SDL2)
    list(APPEND HASCIICAM_AALIB_SOURCES src/aalib/aasdl.c)
endif()

# Conditionally add X11 driver
if(X11_FOUND)
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
if(TARGET SDL2::SDL2)
    target_include_directories(aalib PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

if(X11_FOUND)
    target_include_directories(aalib PRIVATE ${X11_INCLUDE_DIR})
endif()

if(CURSES_FOUND)
    target_include_directories(aalib PRIVATE ${CURSES_INCLUDE_DIR})
endif()

# Main executable
add_executable(hasciicam
    ${HASCIICAM_APP_SOURCES}
    ${HASCIICAM_OUTPUT_SOURCES}
    ${HASCIICAM_RENDER_SOURCES}
    src/hasciicam.c
    ${HASCIICAM_CAPTURE_SOURCES}
)
if(APPLE)
  target_sources(hasciicam PRIVATE src/capture/capture_avfoundation.mm)
endif()
target_compile_definitions(hasciicam PRIVATE
    PACKAGE="hasciicam"
    VERSION="${PROJECT_VERSION}"
)
if(WIN32)
    target_include_directories(hasciicam PRIVATE src/compat)
    target_compile_definitions(hasciicam PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_link_libraries(hasciicam PRIVATE aalib)

# Additional system libraries that may be needed
if(NOT MSVC)
    target_link_libraries(hasciicam PRIVATE m)
endif()

# Link optional libraries to hasciicam
if(TARGET SDL2::SDL2)
  target_link_libraries(hasciicam PRIVATE SDL2::SDL2 SDL2::SDL2main)
endif()

if(X11_FOUND)
  target_link_libraries(hasciicam PRIVATE X11::X11)
endif()

if(Curses_FOUND)
  target_link_libraries(hasciicam PRIVATE ${CURSES_LIBRARIES})
  target_include_directories(hasciicam PRIVATE ${CURSES_INCLUDE_DIRS})
endif()

if(WIN32)
  target_link_libraries(hasciicam PRIVATE mfplat mf mfreadwrite mfuuid ole32 uuid strmiids oleaut32)
endif()

if(APPLE)
  target_link_libraries(hasciicam PRIVATE
    "-framework AVFoundation"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework Foundation"
  )
endif()

# Install targets
install(TARGETS hasciicam DESTINATION bin)
install(FILES doc/hasciicam.1 DESTINATION share/man/man1 OPTIONAL)

if(HASCIICAM_ENABLE_TESTS)
  enable_testing()
  add_executable(test_frame_convert
    tests/test_frame_convert.c
    src/capture/frame_convert.c
  )
  target_include_directories(test_frame_convert PRIVATE src/capture)
  add_test(NAME frame_convert COMMAND test_frame_convert)
endif()
