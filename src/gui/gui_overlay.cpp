#include "gui_overlay.h"

#if defined(HASCIICAM_ENABLE_GUI)

#include <stdio.h>
#include <string.h>

#include "../../third_party/imgui/imgui.h"
#include "../../third_party/imgui/backends/imgui_impl_sdl2.h"
#include "../../third_party/imgui/backends/imgui_impl_sdlrenderer2.h"
#include "../render/render_font.h"

static int g_initialized = 0;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_preview_texture = NULL;
static int g_preview_width = 0;
static int g_preview_height = 0;
static unsigned int g_preview_generation = 0;

static void rgb_to_float3(unsigned int rgb, float out_rgb[3]) {
    out_rgb[0] = ((rgb >> 16) & 0xFF) / 255.0f;
    out_rgb[1] = ((rgb >> 8) & 0xFF) / 255.0f;
    out_rgb[2] = (rgb & 0xFF) / 255.0f;
}

static unsigned int float3_to_rgb(const float in_rgb[3]) {
    int r = (int)(in_rgb[0] * 255.0f + 0.5f);
    int g = (int)(in_rgb[1] * 255.0f + 0.5f);
    int b = (int)(in_rgb[2] * 255.0f + 0.5f);
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b;
}

int hasciicam_gui_overlay_init(SDL_Window *window, SDL_Renderer *renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer))
        return 0;
    if (!ImGui_ImplSDLRenderer2_Init(renderer))
        return 0;
    g_renderer = renderer;
    g_initialized = 1;
    return 1;
}

void hasciicam_gui_overlay_shutdown(void) {
    if (!g_initialized)
        return;
    if (g_preview_texture != NULL) {
        SDL_DestroyTexture(g_preview_texture);
        g_preview_texture = NULL;
    }
    g_renderer = NULL;
    g_preview_width = 0;
    g_preview_height = 0;
    g_preview_generation = 0;
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    g_initialized = 0;
}

static void update_preview_texture(const hasciicam_gui_state *state) {
    int x;
    int y;
    int pitch;
    void *pixels = NULL;
    if (!g_initialized || state == NULL || g_renderer == NULL)
        return;
    if (state->preview_gray == NULL || state->preview_width <= 0 || state->preview_height <= 0)
        return;
    if (g_preview_generation == state->preview_generation)
        return;
    if (g_preview_texture != NULL &&
        (g_preview_width != state->preview_width || g_preview_height != state->preview_height)) {
        SDL_DestroyTexture(g_preview_texture);
        g_preview_texture = NULL;
        g_preview_width = 0;
        g_preview_height = 0;
    }
    if (g_preview_texture == NULL) {
        g_preview_texture = SDL_CreateTexture(g_renderer,
                                              SDL_PIXELFORMAT_ARGB8888,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              state->preview_width,
                                              state->preview_height);
        if (g_preview_texture == NULL)
            return;
        g_preview_width = state->preview_width;
        g_preview_height = state->preview_height;
    }
    if (SDL_LockTexture(g_preview_texture, NULL, &pixels, &pitch) != 0)
        return;
    for (y = 0; y < state->preview_height; ++y) {
        Uint32 *row = (Uint32 *)((unsigned char *)pixels + (size_t)y * (size_t)pitch);
        const unsigned char *src = state->preview_gray + (size_t)y * (size_t)state->preview_stride;
        for (x = 0; x < state->preview_width; ++x) {
            unsigned int g = src[x];
            row[x] = 0xFF000000u | (g << 16) | (g << 8) | g;
        }
    }
    SDL_UnlockTexture(g_preview_texture);
    g_preview_generation = state->preview_generation;
}

void hasciicam_gui_overlay_new_frame(void) {
    if (!g_initialized)
        return;
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

int hasciicam_gui_overlay_process_event(const SDL_Event *event) {
    if (!g_initialized || event == NULL)
        return 0;
    return ImGui_ImplSDL2_ProcessEvent(event) ? 1 : 0;
}

void hasciicam_gui_overlay_draw(hasciicam_gui_state *state) {
    float fg[3];
    float bg[3];
    bool dimmer;
    bool invert;
    bool mirror_x;
    bool mirror_y;

    if (!g_initialized || state == NULL)
        return;
    update_preview_texture(state);

    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("HasciiCam Controls", NULL, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("AA Rendering");
    ImGui::SliderInt("Brightness", &state->aa_bright, 0, 255);
    ImGui::SliderInt("Contrast", &state->aa_contrast, 0, 127);
    ImGui::SliderFloat("Gamma", &state->aa_gamma, 0.5f, 4.0f, "%.2f");
    dimmer = state->aa_dimmer != 0;
    if (ImGui::Checkbox("Dim attributes", &dimmer))
        state->aa_dimmer = dimmer ? 1 : 0;
    invert = state->invert != 0;
    if (ImGui::Checkbox("Invert", &invert))
        state->invert = invert ? 1 : 0;
    {
        int font_count = hasciicam_font_count();
        int selected_index = -1;
        int i;
        for (i = 0; i < font_count; ++i) {
            hasciicam_font_desc desc = hasciicam_font_at(i);
            if (desc.short_name != NULL && strcmp(desc.short_name, state->font) == 0) {
                selected_index = i;
                break;
            }
        }
        if (selected_index < 0 && font_count > 0) {
            hasciicam_font_desc first = hasciicam_font_at(0);
            if (first.short_name != NULL)
                strncpy(state->font, first.short_name, sizeof(state->font) - 1);
            selected_index = 0;
        }
        if (ImGui::BeginCombo("Font", state->font[0] ? state->font : "(none)")) {
            for (i = 0; i < font_count; ++i) {
                hasciicam_font_desc desc = hasciicam_font_at(i);
                char label[128];
                bool is_selected;
                if (desc.short_name == NULL)
                    continue;
                snprintf(label, sizeof(label), "%s (%dx%d)", desc.short_name, 8, desc.height);
                is_selected = (i == selected_index);
                if (ImGui::Selectable(label, is_selected)) {
                    strncpy(state->font, desc.short_name, sizeof(state->font) - 1);
                    state->font[sizeof(state->font) - 1] = '\0';
                    if (strcmp(state->font, state->active_font) != 0)
                        state->font_change_requested = 1;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Separator();
    ImGui::Text("Capture");
    ImGui::BeginGroup();
    mirror_x = state->mirror_x != 0;
    mirror_y = state->mirror_y != 0;
    if (ImGui::Checkbox("Mirror X", &mirror_x))
        state->mirror_x = mirror_x ? 1 : 0;
    if (ImGui::Checkbox("Mirror Y", &mirror_y))
        state->mirror_y = mirror_y ? 1 : 0;
    ImGui::Text("Size: %dx%d", state->capture_width, state->capture_height);
    ImGui::Text("Stride: %d bytes", state->capture_stride_bytes);
    ImGui::Text("Pixel format: %d", (int)state->pixel_format);
    ImGui::EndGroup();
    if (g_preview_texture != NULL && state->preview_width > 0 && state->preview_height > 0) {
        float aspect_w = (float)state->preview_width;
        float aspect_h = (float)state->preview_height;
        float preview_w;
        float preview_h;
        if (state->capture_width > 0 && state->capture_height > 0) {
            aspect_w = (float)state->capture_width;
            aspect_h = (float)state->capture_height;
        }
        if (aspect_w <= 0.0f || aspect_h <= 0.0f) {
            aspect_w = 4.0f;
            aspect_h = 3.0f;
        }
        preview_w = 150.0f;
        preview_h = preview_w * (aspect_h / aspect_w);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - preview_w);
        ImGui::Image((ImTextureID)g_preview_texture, ImVec2(preview_w, preview_h));
    } else {
        ImGui::SameLine();
        ImGui::TextDisabled("No preview yet");
    }
    if (state->capture_control_count > 0) {
        ImGui::Separator();
        ImGui::Text("Camera Controls");
        int i;
        for (i = 0; i < state->capture_control_count; ++i) {
            capture_control_desc *c = &state->capture_controls[i];
            int value = c->current_value;
            if (c->auto_supported) {
                bool is_auto = c->auto_enabled ? true : false;
                char auto_label[80];
                snprintf(auto_label, sizeof(auto_label), "%s auto", c->label ? c->label : c->name);
                if (ImGui::Checkbox(auto_label, &is_auto)) {
                    c->auto_enabled = is_auto ? 1 : 0;
                    state->capture_control_change_requested = 1;
                    state->capture_control_change_is_auto = 1;
                    state->capture_control_change_id = c->id;
                    state->capture_control_change_value = c->auto_enabled;
                }
            }
            if (c->writable && !c->auto_enabled) {
                char slider_label[80];
                snprintf(slider_label, sizeof(slider_label), "%s", c->label ? c->label : c->name);
                if (ImGui::SliderInt(slider_label, &value, c->min_value, c->max_value)) {
                    c->current_value = value;
                    state->capture_control_change_requested = 1;
                    state->capture_control_change_is_auto = 0;
                    state->capture_control_change_id = c->id;
                    state->capture_control_change_value = value;
                }
            } else {
                ImGui::Text("%s: %d", c->label ? c->label : c->name, c->current_value);
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Virtual Camera");
    ImGui::Text("Enabled: %s", state->virtual_camera_enabled ? "yes" : "no");
    ImGui::Text("Backend: %s", state->virtual_camera_backend[0] ? state->virtual_camera_backend : "(none)");
    ImGui::Text("Name: %s", state->virtual_camera_name[0] ? state->virtual_camera_name : "(none)");
    ImGui::Text("Device: %s", state->virtual_camera_device[0] ? state->virtual_camera_device : "(none)");
    ImGui::Text("Size: %dx%d @ %d fps",
                state->virtual_camera_width,
                state->virtual_camera_height,
                state->virtual_camera_fps);
    ImGui::Text("State: %s", state->virtual_camera_connected ? "connected" : "disconnected");
    ImGui::Text("Dropped frames: %llu", state->virtual_camera_dropped_frames);

    ImGui::Separator();
    ImGui::Text("Colors");
    rgb_to_float3(state->foreground_rgb, fg);
    rgb_to_float3(state->background_rgb, bg);
    if (ImGui::ColorEdit3("Foreground", fg))
        state->foreground_rgb = float3_to_rgb(fg);
    if (ImGui::ColorEdit3("Background", bg))
        state->background_rgb = float3_to_rgb(bg);

    ImGui::Separator();
    ImGui::Text("Config");
    ImGui::InputText("Save path", state->save_path, IM_ARRAYSIZE(state->save_path));
    if (ImGui::Button("Save"))
        state->save_requested = 1;

    ImGui::InputText("Load path", state->load_path, IM_ARRAYSIZE(state->load_path));
    ImGui::SameLine();
    if (ImGui::Button("Browse"))
        state->open_load_dialog_requested = 1;
    if (ImGui::Button("Load"))
        state->load_requested = 1;

    if (state->status_message[0] != '\0') {
        if (state->status_is_error)
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", state->status_message);
        else
            ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.35f, 1.0f), "%s", state->status_message);
    }

    ImGui::End();
    ImGui::Render();
}

void hasciicam_gui_overlay_render(SDL_Renderer *renderer) {
    if (!g_initialized || renderer == NULL)
        return;
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
}

int hasciicam_gui_overlay_wants_mouse(void) {
    if (!g_initialized)
        return 0;
    return ImGui::GetIO().WantCaptureMouse ? 1 : 0;
}

int hasciicam_gui_overlay_wants_keyboard(void) {
    if (!g_initialized)
        return 0;
    return ImGui::GetIO().WantCaptureKeyboard ? 1 : 0;
}

#else

int hasciicam_gui_overlay_init(SDL_Window *window, SDL_Renderer *renderer) {
    (void)window;
    (void)renderer;
    return 0;
}
void hasciicam_gui_overlay_shutdown(void) {}
void hasciicam_gui_overlay_new_frame(void) {}
int hasciicam_gui_overlay_process_event(const SDL_Event *event) {
    (void)event;
    return 0;
}
void hasciicam_gui_overlay_draw(hasciicam_gui_state *state) { (void)state; }
void hasciicam_gui_overlay_render(SDL_Renderer *renderer) { (void)renderer; }
int hasciicam_gui_overlay_wants_mouse(void) { return 0; }
int hasciicam_gui_overlay_wants_keyboard(void) { return 0; }

#endif
