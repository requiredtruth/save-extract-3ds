#ifndef SAVE_BACKUP_H
#define SAVE_BACKUP_H

#include <stddef.h>

#define SB_PROFILE_PATH_MAX 1024
#define SB_PROFILE_ID_LENGTH 32

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

typedef struct {
    char id1[SB_PROFILE_ID_LENGTH + 1];
    char id2[SB_PROFILE_ID_LENGTH + 1];
    char title_root[SB_PROFILE_PATH_MAX];
} SaveBackupProfile;

int sb_ensure_directory(const char *path);

int sb_find_profiles(const char *nintendo_3ds_root,
                     SaveBackupProfile *profiles,
                     size_t capacity,
                     size_t *profile_count);

int sb_backup_profile(const SaveBackupProfile *profile,
                      const char *backup_root,
                      SaveBackupStats *stats,
                      SaveBackupLog log_fn,
                      void *log_context);

int sb_restore_profile(const SaveBackupProfile *profile,
                       const char *backup_root,
                       SaveBackupStats *stats,
                       SaveBackupLog log_fn,
                       void *log_context);

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
