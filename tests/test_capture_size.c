#include <stdio.h>

#include "../src/capture/capture_size.h"

static int test_exact_best(void) {
    return capture_size_score(640, 480, 640, 480) <
           capture_size_score(640, 480, 800, 600);
}

static int test_better_choice(void) {
    return capture_size_is_better(640, 480, 1280, 720, 800, 600);
}

static int test_tie_prefers_smaller_area(void) {
    return capture_size_is_better(640, 480, 640, 520, 640, 440);
}

int main(void) {
    if (!test_exact_best()) return 1;
    if (!test_better_choice()) return 1;
    if (!test_tie_prefers_smaller_area()) return 1;
    printf("capture_size tests passed\n");
    return 0;
}
