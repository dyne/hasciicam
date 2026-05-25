#include <hasciicam/hasciicam.h>

int main(void) {
    hasciicam_instance *instance = hasciicam_create();
    if (instance == NULL)
        return 1;
    hasciicam_destroy(instance);
    return 0;
}
