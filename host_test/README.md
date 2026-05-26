# Host test rig for small-ffs

Compiles `fs.c` + `list.c` against a file-backed flash simulator so you can
exercise the FS on a PC. **Bypasses `flash_driver.c`** — the
flash-chip-specific layer is replaced with a thin shim
(`flash_driver_mock.c`) that backs the high-level read / write /
erase / sector-info calls with a binary file. This lets us verify the
wear-leveling, FAT-float, scan and `fs_fclip` logic without emulating
SPI command bytes for each Adesto/Micron/Cypress chip.

## Files

| File | Role |
|---|---|
| `build.bat` | Sets up MSVC, shadows `src/fs.c` (strips `<stdio.h>`/`<stdarg.h>` and force-disables `FILE_PRINTF`), builds `small-ffs-test.exe`. |
| `host_shim.h` | Force-included into fs.c via `/FI`. Renames the stdio overlay (`fopen`→`fs_fopen` etc.) so we don't clash with MSVC's libc. Provides `struct __FILE` so the FS `_FILE` typedef resolves. |
| `flash_driver_mock.c` | Implements `flash_driver_init/read/write/erase_sector_address` + quick-start FAT against a binary file. Enforces NOR-style write semantics (can only flip 1→0). |
| `fs_callbacks.c` | All the scan / format progress / error callbacks the FS calls into. Logs to stderr. |
| `test_main.c` | Format, create files, write log lines, call `fs_fclip` to trim from the front, scan after a simulated power loss. |

## Build & run

From a Developer Command Prompt **or** plain `cmd.exe`:

```cmd
cd small-ffs\host_test
build.bat
```

`build.bat` finds `vcvars64.bat` automatically if it's at one of the
common Visual Studio 2022 locations. The output is `small-ffs-test.exe`
in this directory.

Run it:

```cmd
small-ffs-test.exe flash.bin
```

`flash.bin` is the backing file — defaults to `flash.bin` if you don't
pass an argument. It's a sparse 2 MiB file modelling an Adesto AT25SF161
(2 Mbit) at 256-byte pages, 4 KiB sectors. Delete it to "factory reset"
the simulated chip.

## What the tests cover

1. **Format** — wipes the flash, lays down the initial FAT, asserts
   `fs_fileinfo` reports zero files used.
2. **Round-trip** — creates `app.log`, writes 4 KiB of recognisable
   payload, closes, reopens, reads back, byte-compares.
3. **fs_fclip stress** — appends a known sequence in chunks, clips
   random amounts off the front, verifies the file size and remaining
   payload against a RAM model. This is the test that flushed the
   original 2021 `fs_fclip` bug.
4. **Simulated power loss** — writes a file, then truncates the in-RAM
   FS state (without `fs_fcloseall`), reopens the binary file, calls
   `fs_scan_media`, and asserts the file is recovered intact.

## Adding more tests

`test_main.c` exposes a few small helpers (`assert_file_size`,
`compare_file_to_buf`) — copy the existing test functions and add to
the `run_all_tests` switchboard.
