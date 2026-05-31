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
        "show_help", "show_aahelp", "show_version",
        "quiet", "mode", "device", "input", "size", "pixel_size", "char_size",
        "output_file", "aa_driver", "daemon", "font_size", "font_face",
        "refresh", "aa_bright", "aa_contrast", "aa_gamma", "invert",
        "background", "foreground", "uid", "gid", "frames",
        "sdl_renderer", "sdl_vsync", "fullscreen", "mirror"
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

static void test_default_toml_load(void) {
    hasciicam_config cfg;
    char *argv[] = {
        (char *)"hasciicam"
    };
    int argc = (int)(sizeof(argv) / sizeof(argv[0]));
    const char *path = "hasciicam.toml";
    FILE *fp = fopen(path, "wb");
    expect_true(fp != NULL, "should open default startup toml");
    if (fp) {
        fputs("mode = \"text\"\n", fp);
        fputs("output_file = \"default-config.asc\"\n", fp);
        fputs("aa_bright = 88\n", fp);
        fclose(fp);
    }

    clear_test_env();
    hasciicam_config_init_defaults(&cfg);
    optind = 1;
    hasciicam_config_parse(&cfg, argc, argv, "aahelp", "pkg", "1.0");

    expect_true(cfg.mode == 2, "default toml mode should be text");
    expect_true(strcmp(cfg.aafile, "default-config.asc") == 0,
                "default toml output_file should load");
    expect_true(cfg.aa_bright == 88, "default toml aa_bright should load");

    remove(path);
    clear_test_env();
}

static void test_explicit_toml_precedence(void) {
    hasciicam_config cfg;
    char *argv[] = {
        (char *)"hasciicam",
        (char *)"--config", (char *)"test_app_config_startup.toml",
        (char *)"--mode", (char *)"text",
        (char *)"-o", (char *)"cli.asc"
    };
    int argc = (int)(sizeof(argv) / sizeof(argv[0]));
    FILE *fp = fopen("test_app_config_startup.toml", "wb");
    expect_true(fp != NULL, "should open explicit startup toml");
    if (fp) {
        fputs("mode = \"html\"\n", fp);
        fputs("output_file = \"toml.html\"\n", fp);
        fputs("font_size = 4\n", fp);
        fclose(fp);
    }

    clear_test_env();
    set_env_var("font_size", "3");
    hasciicam_config_init_defaults(&cfg);
    optind = 1;
    hasciicam_config_parse(&cfg, argc, argv, "aahelp", "pkg", "1.0");

    expect_true(cfg.mode == 2, "cli mode should override explicit toml");
    expect_true(strcmp(cfg.aafile, "cli.asc") == 0,
                "cli output_file should override explicit toml");
    expect_true(cfg.fontsize == 3, "env font_size should override explicit toml");

    remove("test_app_config_startup.toml");
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

static void test_toml_mirror_roundtrip(void) {
    hasciicam_config cfg;
    hasciicam_config loaded;
    char err[200];
    const char *path = "test_app_config_mirror.toml";

    hasciicam_config_init_defaults(&cfg);
    cfg.mirror_x = 0;
    cfg.mirror_y = 1;
    expect_true(hasciicam_config_save_toml(&cfg, path, err, sizeof(err)), "save_toml mirror should succeed");

    hasciicam_config_init_defaults(&loaded);
    expect_true(hasciicam_config_load_toml(&loaded, path, err, sizeof(err)), "load_toml mirror should succeed");
    expect_true(loaded.mirror_x == 0, "mirror_x should roundtrip");
    expect_true(loaded.mirror_y == 1, "mirror_y should roundtrip");
    remove(path);
}

static void test_toml_size_key_rejected(void) {
    hasciicam_config cfg;
    char err[200];
    const char *path = "test_app_config_size_alias.toml";
    FILE *fp = fopen(path, "wb");
    expect_true(fp != NULL, "should open size alias toml");
    if (fp) {
        fputs("size = \"80x25\"\n", fp);
        fclose(fp);
    }

    hasciicam_config_init_defaults(&cfg);
    expect_true(!hasciicam_config_load_toml(&cfg, path, err, sizeof(err)), "load_toml should reject size alias");
    remove(path);
}

static void test_env_size_key_ignored(void) {
    hasciicam_config cfg;
    char err[200];

    clear_test_env();
    set_env_var("size", "80x25");
    hasciicam_config_init_defaults(&cfg);
    expect_true(hasciicam_config_load_env(&cfg, err, sizeof(err)), "load_env should ignore size alias");
    expect_true(cfg.explicit_size == 0, "size alias should not set explicit size");
    clear_test_env();
}

int main(void) {
    test_env_load();
    test_cli_overrides_env();
    test_default_toml_load();
    test_explicit_toml_precedence();
    test_toml_roundtrip();
    test_toml_unknown_key();
    test_toml_mirror_roundtrip();
    test_toml_size_key_rejected();
    test_env_size_key_ignored();

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
