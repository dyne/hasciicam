cmake_minimum_required(VERSION 3.12)
project(hasciicam VERSION 2.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(ENABLE_ASAN "Enable AddressSanitizer for debugging" OFF)

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

set(AALIB_SOURCES
    src/aalib/aaattrs.c
    src/aalib/aacurkbd.c
    src/aalib/aacurmou.c
    src/aalib/aacurrfnt.c
    src/aalib/aacurses.c
    src/aalib/aaedit.c
    src/aalib/aafastre.c
    src/aalib/aaflush.c
    src/aalib/aafont.c
    src/aalib/aafonts.c
    src/aalib/aahelp.c
    src/aalib/aaimage.c
    src/aalib/aaimgheight.c
    src/aalib/aaimgwidth.c
    src/aalib/aain.c
    src/aalib/aakbdreg.c
    src/aalib/aalib.c
    src/aalib/aalinux.c
    src/aalib/aalinuxkbd.c
    src/aalib/aamem.c
    src/aalib/aamktabl.c
    src/aalib/aammheight.c
    src/aalib/aammwidth.c
    src/aalib/aamoureg.c
    src/aalib/aaout.c
    src/aalib/aaparse.c
    src/aalib/aaprintf.c
    src/aalib/aaputpixel.c
    src/aalib/aarec.c
    src/aalib/aarecfunc.c
    src/aalib/aaregist.c
    src/aalib/aarender.c
    src/aalib/aasave.c
    src/aalib/aascrheight.c
    src/aalib/aascrwidth.c
    src/aalib/aastdin.c
    src/aalib/aastdout.c
    src/aalib/aatext.c
    src/aalib/font14.c
    src/aalib/font16.c
    src/aalib/font8.c
    src/aalib/font9.c
    src/aalib/fontcour.c
    src/aalib/fontgl.c
    src/aalib/fontline.c
    src/aalib/fontx13b.c
    src/aalib/fontx13.c
    src/aalib/fontx16.c
)

if(WIN32)
    list(APPEND AALIB_SOURCES src/compat/getopt.c)
endif()

# Conditionally add SDL driver
if(TARGET SDL2::SDL2)
    list(APPEND AALIB_SOURCES src/aalib/aasdl.c)
endif()

# Conditionally add X11 driver
if(X11_FOUND)
    list(APPEND AALIB_SOURCES
        src/aalib/aax.c
        src/aalib/aaxkbd.c
        src/aalib/aaxmouse.c
    )
endif()

# Create aalib static library
add_library(aalib STATIC ${AALIB_SOURCES})
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
    src/app/app_config.c
    src/hasciicam.c
    src/capture/capture_backend.c
    src/capture/capture_dshow.cpp
    src/capture/capture_mf.c
    src/capture/frame_convert.c
    src/capture/capture_v4l2.c
)
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

# Install targets
install(TARGETS hasciicam DESTINATION bin)
install(FILES doc/hasciicam.1 DESTINATION share/man/man1 OPTIONAL)
