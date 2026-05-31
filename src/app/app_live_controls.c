#include "app_live_controls.h"

void hasciicam_live_controls_apply(hasciicam_render_session *render_session,
                                   hasciicam_session *capture_session,
                                   hasciicam_config *cfg,
                                   const hasciicam_gui_state *gui_state) {
    if (render_session == NULL || capture_session == NULL || cfg == NULL || gui_state == NULL)
        return;

    hasciicam_gui_state_copy_to_config(gui_state, cfg);
    hasciicam_render_session_apply_tuning(render_session,
                                          gui_state->aa_bright,
                                          gui_state->aa_contrast,
                                          gui_state->aa_gamma);
    if (render_session->render_params != NULL)
        render_session->render_params->inversion = gui_state->invert ? 1 : 0;
    hasciicam_session_set_mirror(capture_session, gui_state->mirror_x, gui_state->mirror_y);
}
