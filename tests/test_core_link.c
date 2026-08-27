#include <hasciicam/hasciicam.h>

int main(void) {
    int (*start_live)(hasciicam_instance *, int, int, int, int, const char *) =
        hasciicam_start_external_live;
    hasciicam_instance *instance = hasciicam_create();
    if (instance == NULL || start_live == NULL)
        return 1;
    hasciicam_destroy(instance);
    return 0;
}
