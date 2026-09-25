#include "save_backup.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_text(const char *path, const char *value) {
    FILE *file;
    char parent[1024];
    char *slash;
    strcpy(parent, path);
    slash = strrchr(parent, '/');
    assert(slash);
    *slash = '\0';
    assert(sb_ensure_directory(parent) == 0);
    file = fopen(path, "wb");
    assert(file);
    assert(fwrite(value, 1, strlen(value), file) == strlen(value));
    assert(fclose(file) == 0);
}

static int exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

static void assert_contents(const char *path, const char *expected) {
    char buffer[64] = {0};
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fread(buffer, 1, sizeof(buffer) - 1, file) == strlen(expected));
    assert(fclose(file) == 0);
    assert(strcmp(buffer, expected) == 0);
}

int main(void) {
    char temp[] = "/tmp/save-extract-3ds-XXXXXX";
    char root[128], backup[128], path[512];
    SaveBackupStats stats;
    const char *id1 = "11111111111111111111111111111111";
    const char *id2 = "22222222222222222222222222222222";
    assert(mkdtemp(temp));
    snprintf(root, sizeof(root), "%s/Nintendo 3DS", temp);
    snprintf(backup, sizeof(backup), "%s/3ds/SaveBackup", temp);

    snprintf(path, sizeof(path), "%s/%s/%s/title/00040000/00030100/data/00000001.sav", root, id1, id2);
    write_text(path, "SAVE-A");
    snprintf(path, sizeof(path), "%s/%s/%s/title/00040000/00030200/data/readme.txt", root, id1, id2);
    write_text(path, "not a save");

    assert(sb_backup_all(root, backup, &stats, NULL, NULL) == 0);
    assert(stats.profiles_found == 1);
    assert(stats.titles_examined == 2);
    assert(stats.titles_copied == 1);
    assert(stats.files_copied == 1);
    snprintf(path, sizeof(path), "%s/00040000/00030100/data/00000001.sav", backup);
    assert(exists(path));
    snprintf(path, sizeof(path), "%s/00040000/00030200/data/readme.txt", backup);
    assert(!exists(path));

    snprintf(path, sizeof(path), "%s/%s/%s/title/00040000/00030100/data/00000001.sav", root, id1, id2);
    write_text(path, "BROKEN");
    assert(sb_restore_all(root, backup, &stats, NULL, NULL) == 0);
    assert(stats.profiles_found == 1);
    assert(stats.files_copied == 1);
    assert_contents(path, "SAVE-A");

    printf("All save backup tests passed.\n");
    return 0;
}
