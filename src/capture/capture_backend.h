#ifndef HASCIICAM_CAPTURE_BACKEND_H
#define HASCIICAM_CAPTURE_BACKEND_H

#include "capture.h"

const capture_ops *capture_default_ops(void);
int capture_open_default(const capture_request *req,
                         capture_device **out_dev,
                         const capture_ops **out_ops);
const char *capture_last_error(void);

#endif
