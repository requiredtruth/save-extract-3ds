#include "save_backup.h"

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SB_PATH_MAX 1024
#define SB_COPY_BUFFER (64U * 1024U)

static void emit(SaveBackupLog fn, void *ctx, const char *fmt, ...) {
    char message[256];
    va_list args;
    if (!fn) return;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    fn(message, ctx);
}

static int path_join(char *out, size_t size, const char *left, const char *right) {
    const size_t len = strlen(left);
    const char *separator = (len > 0 && left[len - 1] == '/') ? "" : "/";
    const int written = snprintf(out, size, "%s%s%s", left, separator, right);
    return written >= 0 && (size_t)written < size ? 0 : -1;
}

static int is_directory(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static int is_regular_file(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

static int is_dot(const char *name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static int is_hex_name(const char *name, size_t length) {
    size_t i;
    if (strlen(name) != length) return 0;
    for (i = 0; i < length; ++i) {
        const char c = name[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return 0;
    }
    return 1;
}

int sb_ensure_directory(const char *path) {
    char partial[SB_PATH_MAX];
    size_t i;
    size_t length;

    if (!path || !*path || strlen(path) >= sizeof(partial)) return -1;
    strcpy(partial, path);
    length = strlen(partial);
    while (length > 1 && partial[length - 1] == '/') partial[--length] = '\0';

    for (i = 1; i <= length; ++i) {
        char saved;
        if (partial[i] != '/' && partial[i] != '\0') continue;
        saved = partial[i];
        partial[i] = '\0';
        if (*partial && partial[strlen(partial) - 1] != ':' &&
            mkdir(partial, 0777) != 0 && errno != EEXIST) {
            partial[i] = saved;
            return -1;
        }
        partial[i] = saved;
    }
    return is_directory(path) ? 0 : -1;
}

static int has_sav_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    return (dot[1] == 's' || dot[1] == 'S') &&
           (dot[2] == 'a' || dot[2] == 'A') &&
           (dot[3] == 'v' || dot[3] == 'V') && dot[4] == '\0';
}

static int tree_contains_sav(const char *directory) {
    DIR *dir = opendir(directory);
    struct dirent *entry;
    char path[SB_PATH_MAX];
    if (!dir) return 0;
    while ((entry = readdir(dir)) != NULL) {
        if (is_dot(entry->d_name) || path_join(path, sizeof(path), directory, entry->d_name) != 0)
            continue;
        if (is_directory(path)) {
            if (tree_contains_sav(path)) { closedir(dir); return 1; }
        } else if (is_regular_file(path) && has_sav_extension(entry->d_name)) {
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

static int copy_file(const char *source, const char *destination, SaveBackupStats *stats) {
    FILE *input = NULL;
    FILE *output = NULL;
    unsigned char *buffer = NULL;
    int result = -1;
    char parent[SB_PATH_MAX];
    char *slash;

    if (strlen(destination) >= sizeof(parent)) goto done;
    strcpy(parent, destination);
    slash = strrchr(parent, '/');
    if (!slash) goto done;
    *slash = '\0';
    if (sb_ensure_directory(parent) != 0) goto done;

    input = fopen(source, "rb");
    if (!input) goto done;
    output = fopen(destination, "wb");
    if (!output) goto done;
    buffer = (unsigned char *)malloc(SB_COPY_BUFFER);
    if (!buffer) goto done;

    for (;;) {
        const size_t got = fread(buffer, 1, SB_COPY_BUFFER, input);
        if (got > 0) {
            if (fwrite(buffer, 1, got, output) != got) goto done;
            stats->bytes_copied += got;
        }
        if (got < SB_COPY_BUFFER) {
            if (ferror(input)) goto done;
            break;
        }
    }
    if (fflush(output) != 0) goto done;
    stats->files_copied++;
    result = 0;

done:
    free(buffer);
    if (output && fclose(output) != 0) result = -1;
    if (input) fclose(input);
    if (result != 0) stats->errors++;
    return result;
}

static int copy_tree(const char *source, const char *destination, SaveBackupStats *stats) {
    DIR *dir;
    struct dirent *entry;
    char source_path[SB_PATH_MAX];
    char destination_path[SB_PATH_MAX];
    int result = 0;

    if (sb_ensure_directory(destination) != 0) { stats->errors++; return -1; }
    dir = opendir(source);
    if (!dir) { stats->errors++; return -1; }
    while ((entry = readdir(dir)) != NULL) {
        if (is_dot(entry->d_name)) continue;
        if (path_join(source_path, sizeof(source_path), source, entry->d_name) != 0 ||
            path_join(destination_path, sizeof(destination_path), destination, entry->d_name) != 0) {
            stats->errors++;
            result = -1;
            continue;
        }
        if (is_directory(source_path)) {
            if (copy_tree(source_path, destination_path, stats) != 0) result = -1;
        } else if (is_regular_file(source_path)) {
            if (copy_file(source_path, destination_path, stats) != 0) result = -1;
        } else {
            stats->files_skipped++;
        }
    }
    closedir(dir);
    return result;
}

typedef int (*ProfileVisitor)(const char *title_root, const char *id1,
                              const char *id2, void *context);

static int visit_profiles(const char *root, SaveBackupStats *stats,
                          ProfileVisitor visitor, void *context) {
    DIR *id1_dir = opendir(root);
    struct dirent *id1_entry;
    char id1_path[SB_PATH_MAX], id2_path[SB_PATH_MAX], title_path[SB_PATH_MAX];
    int result = 0;
    if (!id1_dir) { stats->errors++; return -1; }
    while ((id1_entry = readdir(id1_dir)) != NULL) {
        DIR *id2_dir;
        struct dirent *id2_entry;
        if (!is_hex_name(id1_entry->d_name, 32) ||
            path_join(id1_path, sizeof(id1_path), root, id1_entry->d_name) != 0 ||
            !is_directory(id1_path)) continue;
        id2_dir = opendir(id1_path);
        if (!id2_dir) continue;
        while ((id2_entry = readdir(id2_dir)) != NULL) {
            if (!is_hex_name(id2_entry->d_name, 32) ||
                path_join(id2_path, sizeof(id2_path), id1_path, id2_entry->d_name) != 0 ||
                !is_directory(id2_path) ||
                path_join(title_path, sizeof(title_path), id2_path, "title") != 0 ||
                !is_directory(title_path)) continue;
            stats->profiles_found++;
            if (visitor(title_path, id1_entry->d_name, id2_entry->d_name, context) != 0)
                result = -1;
        }
        closedir(id2_dir);
    }
    closedir(id1_dir);
    return result;
}

typedef struct {
    const char *backup_root;
    SaveBackupStats *stats;
    SaveBackupLog log_fn;
    void *log_context;
} OperationContext;

static int backup_profile(const char *title_root, const char *id1,
                          const char *id2, void *opaque) {
    OperationContext *context = (OperationContext *)opaque;
    char source_root[SB_PATH_MAX], destination_root[SB_PATH_MAX];
    char title_path[SB_PATH_MAX], data_path[SB_PATH_MAX];
    char output_title[SB_PATH_MAX], output_path[SB_PATH_MAX];
    DIR *titles;
    struct dirent *entry;
    int result = 0;

    (void)id1;
    (void)id2;
    if (path_join(source_root, sizeof(source_root), title_root, "00040000") != 0 ||
        !is_directory(source_root)) return 0;
    if (path_join(destination_root, sizeof(destination_root), context->backup_root, "00040000") != 0 ||
        sb_ensure_directory(destination_root) != 0) return -1;
    titles = opendir(source_root);
    if (!titles) { context->stats->errors++; return -1; }

    while ((entry = readdir(titles)) != NULL) {
        if (!is_hex_name(entry->d_name, 8) ||
            path_join(title_path, sizeof(title_path), source_root, entry->d_name) != 0 ||
            !is_directory(title_path) ||
            path_join(data_path, sizeof(data_path), title_path, "data") != 0 ||
            !is_directory(data_path)) continue;
        context->stats->titles_examined++;
        if (!tree_contains_sav(data_path)) continue;
        if (path_join(output_title, sizeof(output_title), destination_root, entry->d_name) != 0 ||
            path_join(output_path, sizeof(output_path), output_title, "data") != 0) {
            context->stats->errors++;
            result = -1;
            continue;
        }
        emit(context->log_fn, context->log_context, "Backing up %s", entry->d_name);
        if (copy_tree(data_path, output_path, context->stats) != 0) result = -1;
        else context->stats->titles_copied++;
    }
    closedir(titles);
    return result;
}

static int restore_profile(const char *title_root, const char *id1,
                           const char *id2, void *opaque) {
    OperationContext *context = (OperationContext *)opaque;
    char source_root[SB_PATH_MAX], destination_root[SB_PATH_MAX];
    (void)id1;
    if (path_join(source_root, sizeof(source_root), context->backup_root, "00040000") != 0 ||
        !is_directory(source_root) ||
        path_join(destination_root, sizeof(destination_root), title_root, "00040000") != 0)
        return -1;
    emit(context->log_fn, context->log_context, "Restoring profile %.8s...", id2);
    return copy_tree(source_root, destination_root, context->stats);
}

static int run_operation(const char *root, const char *backup_root,
                         SaveBackupStats *stats, SaveBackupLog log_fn,
                         void *log_context, ProfileVisitor visitor) {
    OperationContext context;
    if (!root || !backup_root || !stats) return -1;
    memset(stats, 0, sizeof(*stats));
    context.backup_root = backup_root;
    context.stats = stats;
    context.log_fn = log_fn;
    context.log_context = log_context;
    if (sb_ensure_directory(backup_root) != 0) { stats->errors++; return -1; }
    return visit_profiles(root, stats, visitor, &context);
}

int sb_backup_all(const char *root, const char *backup_root, SaveBackupStats *stats,
                  SaveBackupLog log_fn, void *log_context) {
    return run_operation(root, backup_root, stats, log_fn, log_context, backup_profile);
}

int sb_restore_all(const char *root, const char *backup_root, SaveBackupStats *stats,
                   SaveBackupLog log_fn, void *log_context) {
    char source[SB_PATH_MAX];
    if (!backup_root || path_join(source, sizeof(source), backup_root, "00040000") != 0 ||
        !is_directory(source)) {
        if (stats) { memset(stats, 0, sizeof(*stats)); stats->errors = 1; }
        emit(log_fn, log_context, "No 00040000 backup found.");
        return -1;
    }
    return run_operation(root, backup_root, stats, log_fn, log_context, restore_profile);
}
