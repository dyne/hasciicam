#include <hasciicam/hasciicam.h>

int main(void) {
    hasciicam_instance *instance = hasciicam_create();
    if (instance == NULL)
        return 1;
    if (!hasciicam_start_external(instance, 16, 16, 8, 8)) {
        hasciicam_destroy(instance);
        return 2;
    }
    hasciicam_stop(instance);
    hasciicam_destroy(instance);
    return 0;
}
