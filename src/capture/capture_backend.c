#include "capture_backend.h"
#include "capture_mf.h"
#include "capture_v4l2.h"

const capture_ops *capture_default_ops(void) {
#if defined(_WIN32)
    return capture_mf_ops();
#else
    return capture_v4l2_ops();
#endif
}
