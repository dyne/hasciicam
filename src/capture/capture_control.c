#include "capture_control.h"

int capture_controls_list(capture_device *dev,
                          const capture_ops *ops,
                          capture_control_desc *out,
                          int max_controls) {
    if (dev == 0 || ops == 0 || out == 0 || max_controls <= 0)
        return 0;
    if (ops->list_controls == 0)
        return 0;
    return ops->list_controls(dev, out, max_controls);
}

int capture_control_set(capture_device *dev,
                        const capture_ops *ops,
                        capture_control_id id,
                        int value) {
    if (dev == 0 || ops == 0 || ops->set_control == 0)
        return 0;
    return ops->set_control(dev, id, value);
}

int capture_control_set_auto(capture_device *dev,
                             const capture_ops *ops,
                             capture_control_id id,
                             int enabled) {
    if (dev == 0 || ops == 0 || ops->set_control_auto == 0)
        return 0;
    return ops->set_control_auto(dev, id, enabled ? 1 : 0);
}
