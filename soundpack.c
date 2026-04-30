#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static int (*real_open)(const char *pathname, int flags, ...) = NULL;

static char pack_name[256] = "default";

// hot reload tracking
static time_t last_mtime = 0;

// config path
static char config_path[PATH_MAX];
static int config_initialized = 0;

// ----------------------------
// initialize config path + ensure file exists
// ----------------------------
static void init_config() {
    if (config_initialized) return;
    config_initialized = 1;

    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[SOUNDPACK] HOME not set\n");
        return;
    }

    char dir_path[PATH_MAX];

    snprintf(dir_path, sizeof(dir_path),
             "%s/.config/TeamSpeak", home);

    snprintf(config_path, sizeof(config_path),
             "%s/soundpack.conf", dir_path);

    // create directory if needed
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "[SOUNDPACK] failed to create dir: %s\n", dir_path);
            return;
        }
    }

    // create config if missing
    if (stat(config_path, &st) != 0) {
        FILE *create = fopen(config_path, "w");
        if (create) {
            fprintf(create, "pack=default\n");
            fclose(create);
            fprintf(stderr, "[SOUNDPACK] created %s\n", config_path);
        } else {
            fprintf(stderr, "[SOUNDPACK] failed to create config file\n");
        }
    }
}

// ----------------------------
// load config with hot reload
// ----------------------------
static void load_config() {
    init_config();

    struct stat st;
    if (stat(config_path, &st) != 0)
        return;

    // skip if unchanged
    if (st.st_mtime == last_mtime)
        return;

    last_mtime = st.st_mtime;

    FILE *f = fopen(config_path, "r");
    if (!f)
        return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "pack=", 5) == 0) {
            strncpy(pack_name, line + 5, sizeof(pack_name) - 1);

            size_t n = strlen(pack_name);
            if (n && pack_name[n - 1] == '\n')
                pack_name[n - 1] = '\0';

            fprintf(stderr, "[SOUNDPACK] active pack: %s\n", pack_name);
        }
    }

    fclose(f);
}

// ----------------------------
// init real open()
// ----------------------------
static void init() {
    if (!real_open)
        real_open = dlsym(RTLD_NEXT, "open");
}

// ----------------------------
// rewrite /sound/default/ → /sound/<pack>/
// ----------------------------
static const char* rewrite_path(const char *path) {
    static char buffer[4096];

    const char *needle = "/sound/default/";
    char replace[512];

    snprintf(replace, sizeof(replace),
             "/sound/%s/", pack_name);

    const char *pos = strstr(path, needle);
    if (!pos)
        return path;

    size_t before_len = pos - path;

    snprintf(buffer, sizeof(buffer), "%.*s%s%s",
             (int)before_len,
             path,
             replace,
             pos + strlen(needle));

    // ----------------------------
    // fallback: if file doesn't exist, use original
    // ----------------------------
    if (access(buffer, F_OK) != 0) {
        // fallback to default
        return path;
    }

    fprintf(stderr, "[SOUNDPACK] %s -> %s\n", path, buffer);

    return buffer;
}

// ----------------------------
// hook open()
// ----------------------------
int open(const char *pathname, int flags, ...) {
    init();
    load_config(); // now hot-reloading every call

    const char *newpath = rewrite_path(pathname);

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
        return real_open(newpath, flags, mode);
    }

    return real_open(newpath, flags);
}