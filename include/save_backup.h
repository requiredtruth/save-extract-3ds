#ifndef SAVE_BACKUP_H
#define SAVE_BACKUP_H

#include <stddef.h>

typedef void (*SaveBackupLog)(const char *message, void *context);

typedef struct {
    unsigned profiles_found;
    unsigned titles_examined;
    unsigned titles_copied;
    unsigned files_copied;
    unsigned files_skipped;
    unsigned errors;
    unsigned long long bytes_copied;
} SaveBackupStats;

int sb_ensure_directory(const char *path);

int sb_backup_all(const char *nintendo_3ds_root,
                  const char *backup_root,
                  SaveBackupStats *stats,
                  SaveBackupLog log_fn,
                  void *log_context);

int sb_restore_all(const char *nintendo_3ds_root,
                   const char *backup_root,
                   SaveBackupStats *stats,
                   SaveBackupLog log_fn,
                   void *log_context);

#endif
