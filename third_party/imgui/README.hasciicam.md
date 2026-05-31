# Dear ImGui Vendor Notes for HasciiCam

This directory contains the minimal Dear ImGui source set used by HasciiCam's
optional on-screen GUI.

Source reference checkout:

- Local read-only reference: `imgui/`

Copied files:

- `imgui.cpp`
- `imgui_draw.cpp`
- `imgui_tables.cpp`
- `imgui_widgets.cpp`
- `imgui.h`
- `imconfig.h`
- `imgui_internal.h`
- `imstb_rectpack.h`
- `imstb_textedit.h`
- `imstb_truetype.h`
- `backends/imgui_impl_sdl2.cpp`
- `backends/imgui_impl_sdl2.h`
- `backends/imgui_impl_sdlrenderer2.cpp`
- `backends/imgui_impl_sdlrenderer2.h`
- `LICENSE.txt`

Update procedure:

1. Replace only the files listed above from the local `imgui/` reference
   checkout.
2. Keep this vendor note in sync with any file-list changes.
3. Re-run HasciiCam build/tests with `HASCIICAM_ENABLE_GUI=ON` and `OFF`.
