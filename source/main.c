#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "save_backup.h"

#define NINTENDO_ROOT "sdmc:/Nintendo 3DS"
#define BACKUP_ROOT "sdmc:/3ds/SaveBackup"
#define MAX_PROFILES 16
#define APP_VERSION "1.1.0"

static void wait_for_return(void);

static void app_log(const char *message, void *unused) {
    (void)unused;
    printf("%s\n", message);
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

static int choose_profile(SaveBackupProfile *selected) {
    SaveBackupProfile profiles[MAX_PROFILES];
    size_t count = 0;
    size_t visible_count;
    size_t choice = 0;
    size_t i;

    if (sb_find_profiles(NINTENDO_ROOT, profiles, MAX_PROFILES, &count) != 0 || count == 0) {
        consoleClear();
        printf("\x1b[1;1HNo Nintendo 3DS profiles found.\n");
        wait_for_return();
        return 0;
    }
    visible_count = count > MAX_PROFILES ? MAX_PROFILES : count;
    if (visible_count == 1) {
        *selected = profiles[0];
        return 1;
    }

    while (aptMainLoop()) {
        u32 down;
        size_t first = choice > 5 ? choice - 5 : 0;
        consoleClear();
        printf("\x1b[1;1HSELECT SD PROFILE\n\n");
        for (i = first; i < visible_count && i < first + 8; ++i) {
            printf("%c %s / %s\n", i == choice ? '>' : ' ',
                   profiles[i].id1 + 24, profiles[i].id2 + 24);
        }
        if (count > MAX_PROFILES)
            printf("\nShowing first %u of %u profiles.\n",
                   (unsigned)MAX_PROFILES, (unsigned)count);
        printf("\nA: Select    B: Cancel\n");
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
        hidScanInput();
        down = hidKeysDown();
        if (down & KEY_B) return 0;
        if (down & KEY_UP) choice = choice == 0 ? visible_count - 1 : choice - 1;
        if (down & KEY_DOWN) choice = (choice + 1) % visible_count;
        if (down & KEY_A) {
            *selected = profiles[choice];
            return 1;
        }
    }
    return 0;
}

static void wait_for_return(void) {
    printf("\nPress B to return to the menu.\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_B) return;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
}

static void show_result(const char *name, int result, const SaveBackupStats *stats) {
    printf("\n%s %s\n", name, result == 0 && stats->errors == 0 ? "complete." : "finished with errors.");
    printf("Profiles: %u\n", stats->profiles_found);
    if (strcmp(name, "Backup") == 0) {
        printf("Titles checked: %u\n", stats->titles_examined);
        printf("Titles backed up: %u\n", stats->titles_copied);
    }
    printf("Files copied: %u\n", stats->files_copied);
    printf("Data copied: %llu bytes\n", stats->bytes_copied);
    printf("Errors: %u\n", stats->errors);
}

static int confirm_restore(const SaveBackupProfile *profile) {
    consoleClear();
    printf("\x1b[1;1HRESTORE SAVED DATA\n\n");
    printf("This overwrites matching files in one profile:\n");
    printf("ID1 ...%s\nID2 ...%s\n\n", profile->id1 + 24, profile->id2 + 24);
    printf("Hold L + R and press A to continue.\n");
    printf("Press B to cancel.\n");
    while (aptMainLoop()) {
        u32 held;
        hidScanInput();
        held = hidKeysHeld();
        if (hidKeysDown() & KEY_B) return 0;
        if ((hidKeysDown() & KEY_A) && (held & KEY_L) && (held & KEY_R)) return 1;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    return 0;
}

static void run_backup(void) {
    SaveBackupProfile profile;
    SaveBackupStats stats;
    int result;
    if (!choose_profile(&profile)) return;
    consoleClear();
    printf("\x1b[1;1HBACKUP\n\nProfile ...%s\nScanning SD card...\n",
           profile.id2 + 24);
    result = sb_backup_profile(&profile, BACKUP_ROOT, &stats, app_log, NULL);
    show_result("Backup", result, &stats);
    wait_for_return();
}

static void run_restore(void) {
    SaveBackupProfile profile;
    SaveBackupStats stats;
    int result;
    if (!choose_profile(&profile) || !confirm_restore(&profile)) return;
    consoleClear();
    printf("\x1b[1;1HRESTORE\n\nCopying backup...\n");
    result = sb_restore_profile(&profile, BACKUP_ROOT, &stats, app_log, NULL);
    show_result("Restore", result, &stats);
    wait_for_return();
}

static void draw_menu(int selection, const char *boot_error) {
    consoleClear();
    printf("\x1b[1;1HSave Extract 3DS v%s\n", APP_VERSION);
    printf("Raw save-data backup and restore\n\n");
    printf(" %c Backup\n", selection == 0 ? '>' : ' ');
    printf(" %c Restore\n", selection == 1 ? '>' : ' ');
    printf("\nD-Pad: Select    A: Open    START: Exit\n");
    printf("\nBackup folder:\n%s\n", BACKUP_ROOT);
    if (boot_error) printf("\nERROR: %s\n", boot_error);
}

int main(void) {
    int selection = 0;
    const char *boot_error = NULL;

    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    if (sb_ensure_directory(BACKUP_ROOT) != 0)
        boot_error = "Could not create SaveBackup.";

    draw_menu(selection, boot_error);
    while (aptMainLoop()) {
        u32 down;
        hidScanInput();
        down = hidKeysDown();
        if (down & KEY_START) break;
        if (down & (KEY_UP | KEY_DOWN)) {
            selection = 1 - selection;
            draw_menu(selection, boot_error);
        }
        if ((down & KEY_A) && !boot_error) {
            if (selection == 0) run_backup();
            else run_restore();
            draw_menu(selection, boot_error);
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
