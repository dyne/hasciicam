#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/app/app_config.h"
#include "../src/compat/getopt.h"

#if defined(_WIN32)
#include <windows.h>
#endif

static int failures = 0;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static void set_env_var(const char *key, const char *value) {
#if defined(_WIN32)
    _putenv_s(key, value ? value : "");
#else
    if (value == NULL) {
        unsetenv(key);
    } else {
        setenv(key, value, 1);
    }
#endif
}

static void unset_env_var(const char *key) {
#if defined(_WIN32)
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
}

static void clear_test_env(void) {
    const char *keys[] = {
        "mode", "output_file", "font_size", "invert", "sdl_vsync", "char_size", "refresh"
    };
    size_t i;
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        unset_env_var(keys[i]);
    }
}

static void test_env_load(void) {
    hasciicam_config cfg;
    char err[200];

    clear_test_env();
    set_env_var("mode", "html");
    set_env_var("output_file", "env.html");
    set_env_var("font_size", "3");
    set_env_var("invert", "true");
    set_env_var("sdl_vsync", "off");
    set_env_var("char_size", "80x25");

    hasciicam_config_init_defaults(&cfg);
    expect_true(hasciicam_config_load_env(&cfg, err, sizeof(err)), "load_env should succeed");
    expect_true(cfg.mode == 1, "mode should be html");
    expect_true(strcmp(cfg.aafile, "env.html") == 0, "output_file should be env.html");
    expect_true(cfg.fontsize == 3, "font_size should be 3");
    expect_true(cfg.invert == 1, "invert should be true");
    expect_true(cfg.sdl_vsync == 0, "sdl_vsync should be off");
    expect_true(cfg.size_intent == HASCIICAM_SIZE_CHARS, "char_size should set char intent");
    expect_true(cfg.size_w == 80 && cfg.size_h == 25, "char_size should parse 80x25");

    clear_test_env();
}

static void test_cli_overrides_env(void) {
    hasciicam_config cfg;
    char *argv[] = {
        (char *)"hasciicam",
        (char *)"--mode", (char *)"text",
        (char *)"--font-size", (char *)"2",
        (char *)"-o", (char *)"cli.html"
    };
    int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    clear_test_env();
    set_env_var("mode", "html");
    set_env_var("font_size", "4");
    set_env_var("output_file", "env.html");

    hasciicam_config_init_defaults(&cfg);
    optind = 1;
    hasciicam_config_parse(&cfg, argc, argv, "aahelp", "pkg", "1.0");

    expect_true(cfg.mode == 2, "cli mode should override env mode");
    expect_true(cfg.fontsize == 2, "cli font_size should override env font_size");
    expect_true(strcmp(cfg.aafile, "cli.html") == 0, "cli output_file should override env output_file");

    clear_test_env();
}

static void test_toml_roundtrip(void) {
    hasciicam_config cfg;
    hasciicam_config loaded;
    char err[200];
    const char *path = "test_app_config_roundtrip.toml";

    hasciicam_config_init_defaults(&cfg);
    cfg.mode = 2;
    cfg.explicit_output = 1;
    strcpy(cfg.aafile, "roundtrip.asc");
    cfg.sdl_vsync = 1;
    cfg.invert = 1;
    cfg.size_intent = HASCIICAM_SIZE_CHARS;
    cfg.size_w = 90;
    cfg.size_h = 30;
    cfg.explicit_size = 1;

    expect_true(hasciicam_config_save_toml(&cfg, path, err, sizeof(err)), "save_toml should succeed");

    hasciicam_config_init_defaults(&loaded);
    expect_true(hasciicam_config_load_toml(&loaded, path, err, sizeof(err)), "load_toml should succeed");
    expect_true(loaded.mode == 2, "roundtrip mode should be text");
    expect_true(strcmp(loaded.aafile, "roundtrip.asc") == 0, "roundtrip output_file should match");
    expect_true(loaded.sdl_vsync == 1, "roundtrip sdl_vsync should be on");
    expect_true(loaded.invert == 1, "roundtrip invert should match");
    expect_true(loaded.size_intent == HASCIICAM_SIZE_CHARS, "roundtrip size intent should be chars");
    expect_true(loaded.size_w == 90 && loaded.size_h == 30, "roundtrip size should match");

    remove(path);
}

static void test_toml_unknown_key(void) {
    hasciicam_config cfg;
    char err[200];
    const char *path = "test_app_config_bad.toml";
    FILE *fp = fopen(path, "wb");
    expect_true(fp != NULL, "should open bad toml file");
    if (fp) {
        fputs("unknown_key = 1\n", fp);
        fclose(fp);
    }

    hasciicam_config_init_defaults(&cfg);
    expect_true(!hasciicam_config_load_toml(&cfg, path, err, sizeof(err)), "load_toml should reject unknown key");
    remove(path);
}

int main(void) {
    test_env_load();
    test_cli_overrides_env();
    test_toml_roundtrip();
    test_toml_unknown_key();

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
