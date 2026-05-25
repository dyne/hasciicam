#include "capture_backend.h"
#include "capture_v4l2.h"

const capture_ops *capture_default_ops(void) {
    return capture_v4l2_ops();
}
