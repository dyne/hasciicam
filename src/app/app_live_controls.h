#ifndef HASCIICAM_APP_LIVE_CONTROLS_H
#define HASCIICAM_APP_LIVE_CONTROLS_H

#include "app_config.h"
#include "app_session.h"
#include "../gui/gui_state.h"
#include "../render/render_session.h"

/**
 * Apply GUI runtime values to render session, capture session, and config.
 */
void hasciicam_live_controls_apply(hasciicam_render_session *render_session,
                                   hasciicam_session *capture_session,
                                   hasciicam_config *cfg,
                                   const hasciicam_gui_state *gui_state);

#endif
