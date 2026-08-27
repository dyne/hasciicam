#include <aalib.h>

static int flush_calls;
static int flush_result;

static void fake_flush(aa_context *context) {
    (void)context;
    flush_calls += 1;
}

static int fake_flush_status(aa_context *context) {
    (void)context;
    return flush_result;
}

int main(void) {
    aa_context context = {0};
    aa_driver driver = {0};

    driver.flush = fake_flush;
    driver.flush_status = fake_flush_status;
    context.driver = &driver;

    flush_result = 0;
    if (aa_flush_checked(&context) != 0 || flush_calls != 1)
        return 1;
    flush_result = 1;
    if (aa_flush_checked(&context) != 1 || flush_calls != 2)
        return 2;
    if (aa_flush_checked(NULL) != 0)
        return 3;
    return 0;
}
