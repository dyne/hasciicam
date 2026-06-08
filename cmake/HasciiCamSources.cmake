set(HASCIICAM_AALIB_SOURCES
    third_party/aalib/aaattrs.c
    third_party/aalib/aacurkbd.c
    third_party/aalib/aacurmou.c
    third_party/aalib/aacurrfnt.c
    third_party/aalib/aacurses.c
    third_party/aalib/aaedit.c
    third_party/aalib/aafastre.c
    third_party/aalib/aaflush.c
    third_party/aalib/aafont.c
    third_party/aalib/aafonts.c
    third_party/aalib/aahelp.c
    third_party/aalib/aaimage.c
    third_party/aalib/aaimgheight.c
    third_party/aalib/aaimgwidth.c
    third_party/aalib/aain.c
    third_party/aalib/aakbdreg.c
    third_party/aalib/aalib.c
    third_party/aalib/aalinux.c
    third_party/aalib/aalinuxkbd.c
    third_party/aalib/aamem.c
    third_party/aalib/aamktabl.c
    third_party/aalib/aammheight.c
    third_party/aalib/aammwidth.c
    third_party/aalib/aamoureg.c
    third_party/aalib/aaout.c
    third_party/aalib/aaparse.c
    third_party/aalib/aaprintf.c
    third_party/aalib/aaputpixel.c
    third_party/aalib/aarec.c
    third_party/aalib/aarecfunc.c
    third_party/aalib/aaregist.c
    third_party/aalib/aarender.c
    third_party/aalib/aasave.c
    third_party/aalib/aascrheight.c
    third_party/aalib/aascrwidth.c
    third_party/aalib/aastdin.c
    third_party/aalib/aastdout.c
    third_party/aalib/aatext.c
    third_party/aalib/font14.c
    third_party/aalib/font16.c
    third_party/aalib/font8.c
    third_party/aalib/font9.c
    third_party/aalib/fontcour.c
    third_party/aalib/fontgl.c
    third_party/aalib/fontline.c
    third_party/aalib/fontx13b.c
    third_party/aalib/fontx13.c
    third_party/aalib/fontx16.c
    src/render/render_font.c
)

set(HASCIICAM_APP_SOURCES
    src/app/app_config.c
    src/app/app_live_controls.c
    src/app/app_virtual_camera.c
    src/app/app_size.c
    src/app/app_session.c
    src/display/display_size.c
    src/gui/gui_bridge_stub.c
    src/gui/gui_state.c
    src/public/hasciicam_api.c
    src/virtual_camera/virtual_camera.c
    src/virtual_camera/virtual_camera_convert.c
)

set(HASCIICAM_OUTPUT_SOURCES
    src/output/output.c
    src/output/output_file.c
    src/output/output_memory.c
)

set(HASCIICAM_RENDER_SOURCES
    src/render/render_session.c
)

set(HASCIICAM_CAPTURE_SOURCES
    src/capture/capture_backend.c
    src/capture/capture_control.c
    src/capture/capture_external.c
    src/capture/capture_size.c
    src/capture/capture_synthetic.c
    src/capture/capture_dshow.cpp
    src/capture/capture_mf.c
    src/capture/frame_convert.c
    src/capture/capture_v4l2.c
)

if(WIN32)
    list(APPEND HASCIICAM_AALIB_SOURCES src/compat/getopt.c)
    list(APPEND HASCIICAM_APP_SOURCES src/virtual_camera/windows/pipe/hasciicam_virtual_camera_pipe.c)
    list(APPEND HASCIICAM_APP_SOURCES src/virtual_camera/windows/pipe/hasciicam_virtual_camera_windows.c)
    list(APPEND HASCIICAM_APP_SOURCES src/virtual_camera/windows/hasciicam_app_virtual_camera_windows.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND HASCIICAM_APP_SOURCES src/virtual_camera/linux/hasciicam_virtual_camera_v4l2.c)
endif()
