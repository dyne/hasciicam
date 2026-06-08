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
        "virtual_camera", "virtual_camera_device", "virtual_camera_size", "virtual_camera_fps",
        "output_file", "aa_driver", "daemon", "font_size", "font_face", "font",
        "refresh", "aa_bright", "aa_contrast", "aa_gamma", "aa_dimmer", "invert",
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
    set_env_var("font", "vga8");
    set_env_var("invert", "true");
    set_env_var("aa_dimmer", "off");
    set_env_var("sdl_vsync", "off");
    set_env_var("char_size", "80x25");
    set_env_var("virtual_camera", "true");
    set_env_var("virtual_camera_device", "HasciiCam");
    set_env_var("virtual_camera_size", "1280x720");
    set_env_var("virtual_camera_fps", "30");

    hasciicam_config_init_defaults(&cfg);
    expect_true(hasciicam_config_load_env(&cfg, err, sizeof(err)), "load_env should succeed");
    expect_true(cfg.mode == 1, "mode should be html");
    expect_true(strcmp(cfg.aafile, "env.html") == 0, "output_file should be env.html");
    expect_true(cfg.fontsize == 3, "font_size should be 3");
    expect_true(strcmp(cfg.font, "vga8") == 0, "font should be vga8");
    expect_true(cfg.invert == 1, "invert should be true");
    expect_true(cfg.aa_dimmer == 0, "aa_dimmer should be off");
    expect_true(cfg.sdl_vsync == 0, "sdl_vsync should be off");
    expect_true(cfg.size_intent == HASCIICAM_SIZE_CHARS, "char_size should set char intent");
    expect_true(cfg.size_w == 80 && cfg.size_h == 25, "char_size should parse 80x25");
    expect_true(cfg.virtual_camera == 1, "virtual_camera should be true");
    expect_true(strcmp(cfg.virtual_camera_device, "HasciiCam") == 0,
                "virtual_camera_device should be HasciiCam");
    expect_true(cfg.virtual_camera_width == 1280 && cfg.virtual_camera_height == 720,
                "virtual_camera_size should parse 1280x720");
    expect_true(cfg.virtual_camera_fps == 30, "virtual_camera_fps should be 30");

    clear_test_env();
}

static void test_defaults(void) {
    hasciicam_config cfg;

    hasciicam_config_init_defaults(&cfg);
    expect_true(strcmp(cfg.background, "000000") == 0, "default background should be black");
    expect_true(strcmp(cfg.foreground, "FFFFFF") == 0, "default foreground should be white");
    expect_true(cfg.aa_dimmer == 1, "default aa_dimmer should be on");
    expect_true(cfg.virtual_camera == 0, "default virtual_camera should be off");
#if defined(_WIN32)
    expect_true(strcmp(cfg.virtual_camera_device, "") == 0,
                "default virtual_camera_device should be empty on Windows");
#else
    expect_true(strcmp(cfg.virtual_camera_device, "/dev/video10") == 0,
                "default virtual_camera_device should be /dev/video10 on Linux");
#endif
    expect_true(cfg.virtual_camera_width == 1280 && cfg.virtual_camera_height == 720,
                "default virtual_camera_size should be 1280x720");
    expect_true(cfg.virtual_camera_fps == 30, "default virtual_camera_fps should be 30");
}

static void test_cli_overrides_env(void) {
    hasciicam_config cfg;
    char *argv[] = {
        (char *)"hasciicam",
        (char *)"--mode", (char *)"text",
        (char *)"--font-size", (char *)"2",
        (char *)"--font", (char *)"vga9",
        (char *)"-o", (char *)"cli.html"
    };
    int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    clear_test_env();
    set_env_var("mode", "html");
    set_env_var("font_size", "4");
    set_env_var("font", "vga8");
    set_env_var("output_file", "env.html");

    hasciicam_config_init_defaults(&cfg);
    optind = 1;
    hasciicam_config_parse(&cfg, argc, argv, "aahelp", "pkg", "1.0");

    expect_true(cfg.mode == 2, "cli mode should override env mode");
    expect_true(cfg.fontsize == 2, "cli font_size should override env font_size");
    expect_true(strcmp(cfg.font, "vga9") == 0, "cli font should override env font");
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
        fputs("font = \"courier\"\n", fp);
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
    expect_true(strcmp(cfg.font, "courier") == 0, "default toml font should load");

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
    set_env_var("font", "vga8");
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
    cfg.aa_dimmer = 0;
    cfg.size_intent = HASCIICAM_SIZE_CHARS;
    cfg.size_w = 90;
    cfg.size_h = 30;
    cfg.explicit_size = 1;
    strcpy(cfg.font, "vga8");
    cfg.virtual_camera = 1;
    cfg.virtual_camera_width = 1280;
    cfg.virtual_camera_height = 720;
    cfg.virtual_camera_fps = 30;
    strcpy(cfg.virtual_camera_device, "HasciiCam");

    expect_true(hasciicam_config_save_toml(&cfg, path, err, sizeof(err)), "save_toml should succeed");

    hasciicam_config_init_defaults(&loaded);
    expect_true(hasciicam_config_load_toml(&loaded, path, err, sizeof(err)), "load_toml should succeed");
    expect_true(loaded.mode == 2, "roundtrip mode should be text");
    expect_true(strcmp(loaded.aafile, "roundtrip.asc") == 0, "roundtrip output_file should match");
    expect_true(loaded.sdl_vsync == 1, "roundtrip sdl_vsync should be on");
    expect_true(loaded.invert == 1, "roundtrip invert should match");
    expect_true(loaded.aa_dimmer == 0, "roundtrip aa_dimmer should match");
    expect_true(loaded.size_intent == HASCIICAM_SIZE_CHARS, "roundtrip size intent should be chars");
    expect_true(loaded.size_w == 90 && loaded.size_h == 30, "roundtrip size should match");
    expect_true(strcmp(loaded.font, "vga8") == 0, "roundtrip font should match");
    expect_true(loaded.virtual_camera == 1, "roundtrip virtual_camera should match");
    expect_true(strcmp(loaded.virtual_camera_device, "HasciiCam") == 0,
                "roundtrip virtual_camera_device should match");
    expect_true(loaded.virtual_camera_width == 1280 && loaded.virtual_camera_height == 720,
                "roundtrip virtual_camera_size should match");
    expect_true(loaded.virtual_camera_fps == 30, "roundtrip virtual_camera_fps should match");

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

static void test_toml_invalid_font(void) {
    hasciicam_config cfg;
    char err[200];
    const char *path = "test_app_config_bad_font.toml";
    FILE *fp = fopen(path, "wb");
    expect_true(fp != NULL, "should open bad font toml file");
    if (fp) {
        fputs("font = \"missing-font\"\n", fp);
        fclose(fp);
    }

    hasciicam_config_init_defaults(&cfg);
    expect_true(!hasciicam_config_load_toml(&cfg, path, err, sizeof(err)),
                "load_toml should reject invalid font");
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

static void test_env_virtual_camera_size_rejected(void) {
    hasciicam_config cfg;
    char err[200];

    clear_test_env();
    set_env_var("virtual_camera_size", "1279x720");
    hasciicam_config_init_defaults(&cfg);
    expect_true(!hasciicam_config_load_env(&cfg, err, sizeof(err)),
                "load_env should reject odd virtual_camera_size");
    clear_test_env();
}

int main(void) {
    test_defaults();
    test_env_load();
    test_cli_overrides_env();
    test_default_toml_load();
    test_explicit_toml_precedence();
    test_toml_roundtrip();
    test_toml_unknown_key();
    test_toml_invalid_font();
    test_toml_mirror_roundtrip();
    test_toml_size_key_rejected();
    test_env_size_key_ignored();
    test_env_virtual_camera_size_rejected();

    if (failures) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    return 0;
}


