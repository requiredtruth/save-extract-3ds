# Save Extract 3DS

Save Extract 3DS is a Nintendo 3DS Homebrew Launcher application that backs up
and restores raw Nintendo 3DS title save-data directories.

## What it does

- Creates `sdmc:/3ds/SaveBackup` when the app starts.
- Lets the user select the exact ID1/ID2 profile when more than one exists.
- **Backup** scans every profile under
  `sdmc:/Nintendo 3DS/<id1>/<id2>/title/00040000`.
- A title is backed up only when its `data` directory contains at least one
  `.sav` file (including nested directories).
- Matching data is copied to
  `sdmc:/3ds/SaveBackup/00040000/<title-id>/data`.
- Backup creation is transactional: files are built in a working directory and
  the previous good backup is retained if scanning or copying fails.
- Each successful backup is a fresh snapshot, so deleted/stale title folders do
  not remain in the new backup.
- **Restore** copies the saved `00040000` tree into only the selected
  `<id1>/<id2>/title` directory and overwrites matching files.

## Important limitations

This is a raw encrypted SD-card backup, not a decrypted or portable save
export. Nintendo 3DS SD data is tied to the source system/SD encryption context,
and some games use additional secure-value protections. Keep another copy of
the SD card before restoring. Do not remove the SD card or power off while a
copy is running.

## Install

1. Download `save-extract-3ds.3dsx` from the latest GitHub Release.
2. Put it at `sd:/3ds/save-extract-3ds/save-extract-3ds.3dsx`.
3. Launch it from the Homebrew Launcher.

## Controls

- D-Pad Up/Down: choose Backup or Restore
- A: run the selected function
- Restore confirmation: hold L + R, then press A
- B: cancel confirmation/return to menu
- START: exit

## Build

Install devkitPro with the `3ds-dev` group, then run:

```sh
make
```

The output is `save-extract-3ds.3dsx`. Host-side filesystem tests can be run
without devkitPro:

```sh
make test
```

## License

MIT
