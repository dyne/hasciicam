cmake_minimum_required(VERSION 3.10)
project(hasciicam VERSION 1.3 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(ENABLE_ASAN "Enable AddressSanitizer for debugging" OFF)

# Find required libraries
find_package(PkgConfig REQUIRED)

# Check for SDL2
pkg_check_modules(SDL2 sdl2)
if(SDL2_FOUND)
    message(STATUS "Found SDL2: ${SDL2_VERSION}")
    add_compile_definitions(SDL_DRIVER)
else()
    message(WARNING "SDL2 not found - SDL driver will not be available")
endif()

# Check for X11
find_package(X11)
if(X11_FOUND)
    message(STATUS "Found X11")
    add_compile_definitions(X11_DRIVER)
else()
    message(WARNING "X11 not found - X11 driver will not be available")
endif()

# Check for Curses
find_package(Curses)
if(CURSES_FOUND)
    message(STATUS "Found Curses")
    add_compile_definitions(CURSES_DRIVER)
else()
    message(WARNING "Curses not found - Curses driver will not be available")
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
    src/aalib/aafire.c
    src/aalib/aaflush.c
    src/aalib/aafont.c
    src/aalib/aafonts.c
    src/aalib/aahelp.c
    src/aalib/aaimage.c
    src/aalib/aaimgheight.c
    src/aalib/aaimgwidth.c
    src/aalib/aain.c
    src/aalib/aainfo.c
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
    src/aalib/aasavefont.c
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

# Conditionally add SDL driver
if(SDL2_FOUND)
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

# Link libraries to aalib
if(SDL2_FOUND)
    target_include_directories(aalib PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(aalib PRIVATE ${SDL2_LIBRARIES})
endif()

if(X11_FOUND)
    target_include_directories(aalib PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(aalib PRIVATE ${X11_LIBRARIES})
endif()

if(CURSES_FOUND)
    target_include_directories(aalib PRIVATE ${CURSES_INCLUDE_DIR})
    target_link_libraries(aalib PRIVATE ${CURSES_LIBRARIES})
endif()

# Main executable
add_executable(hasciicam src/hasciicam.c)
target_compile_definitions(hasciicam PRIVATE
    PACKAGE="hasciicam"
    VERSION="${PROJECT_VERSION}"
)
target_link_libraries(hasciicam PRIVATE aalib)

# Additional system libraries that may be needed
target_link_libraries(hasciicam PRIVATE m)

# Install targets
install(TARGETS hasciicam DESTINATION bin)
install(FILES doc/hasciicam.1 DESTINATION share/man/man1 OPTIONAL)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  SDL2 support:    ${SDL2_FOUND}")
message(STATUS "  X11 support:     ${X11_FOUND}")
message(STATUS "  Curses support:  ${CURSES_FOUND}")
message(STATUS "  ASAN enabled:    ${ENABLE_ASAN}")
message(STATUS "")
