#ifndef HASCIICAM_CAPTURE_CONTROL_H
#define HASCIICAM_CAPTURE_CONTROL_H

#include "capture.h"

int capture_controls_list(capture_device *dev,
                          const capture_ops *ops,
                          capture_control_desc *out,
                          int max_controls);
int capture_control_set(capture_device *dev,
                        const capture_ops *ops,
                        capture_control_id id,
                        int value);
int capture_control_set_auto(capture_device *dev,
                             const capture_ops *ops,
                             capture_control_id id,
                             int enabled);

#endif
