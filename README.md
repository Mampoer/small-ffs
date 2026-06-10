# small-ffs

A small flash file system for embedded SPI NOR flash. Originally written
in 2009 for an STM32F10x superloop project and progressively hardened
since: multi-chip support, wear-leveled FAT, per-sector erase counters,
and the ability to **clip bytes off the *front*** of a file without
rewriting it (so log files can be uploaded oldest-first and trimmed in
place).

License: BSD 3-clause — see [LICENSE](LICENSE).

## What it does (and what makes it different)

| Goal | How |
|---|---|
| Don't burn out the FAT | The FAT itself is a regular file chain that floats across pages. Each sector tracks its own erase count (`sector_info_t`, 24-bit count + 8-bit key byte at the head of every sector). New FAT pages are allocated to the lowest-erase-count sector below the wear threshold. |
| Reclaim log space oldest-first | `fs_fclip(name, n)` frees the leading pages and advances `start_offset` — `O(pages_clipped)` write cost, no full-file rewrite. |
| Survive power loss | Boot calls `fs_scan_media()` which walks every page, rebuilds the in-RAM picture from page headers, removes dangling fragments and integrity-failed file chains. `flash_driver_save_quick_start_fat` caches the active FAT page so subsequent boots can skip the full scan. |
| Run on different SPI NOR parts | The driver auto-detects by JEDEC ID at init and fills the runtime geometry (`flash_page_size`, `flash_pages_per_sector`, `file_system_start_page`, `file_system_end_page`). Supported today: Adesto AT25DL161 / AT25SF161 / AT25SL128, Micron MT25QL256, Cypress S25FL127S. |

## Layout

```
inc/
  fs.h               Public API + on-disk types
  flash_driver.h     Wear thresholds + runtime geometry
  flash_hw.h         SPI primitives the driver expects from the platform
  ffs_config.h       MAX_OPEN_FILES, MAX_FNAME_SIZE, etc.
  list.h             Small singly-linked-list helper used for open-file tracking

src/
  fs.c               The file system. Singleton fs_t holds open-file list,
                     current FAT, wear threshold, etc.
  flash_driver.c     SPI flash driver. Multi-chip JEDEC-ID detect, verify-
                     after-write, 4K + 64K erase, quick-start FAT cache.
  list.c             list_t implementation
  fs_test.c          On-target stress tests (run on hardware via Keil)

host_test/           Build fs.c on a PC against a binary-file flash mock.
                     See host_test/README.md for the build script.

project/             Keil µVision 5 project for the original STM32F10x demo.
```

## The Keil stdio overlay (important!)

This FS is designed to **be the C standard I/O on Keil ARMCC**. Application
code includes `<stdio.h>` and calls `fopen` / `fputc` / `fgetc` / `fputs` /
`fgets` / `fread` / `fwrite` / `fseek` / `fclose` / `remove` / `rename` —
the linker resolves those names to the implementations in `fs.c`, replacing
Keil's libc defaults.

The trick is that `fs.h` defines the file-handle struct as
`struct __FILE { ... } _FILE;`. Keil's `<stdio.h>` forward-declares
`typedef struct __FILE FILE;` — same struct tag, so once `fs.h` is included
the application's `FILE *` and the FS's internal handle are literally the
same type. No casting, no `fs_*` prefixed wrapper API.

This *only* works on toolchains that use `struct __FILE` for `FILE`:

| Toolchain | `FILE` is | Overlay works? |
|---|---|---|
| Keil ARMCC | `struct __FILE` | ✅ |
| Newlib (arm-none-eabi-gcc) | `struct __sFILE` | ❌ — clash |
| glibc | `struct _IO_FILE` | ❌ — clash |
| MSVC libc | `struct _iobuf` | ❌ — clash |

For non-Keil builds, `host_test/host_shim.h` shows the pattern: don't
`#include <stdio.h>` in the same TU as `fs.h`, and remap `fopen`→`fs_fopen`
etc. via `#define` so the FS's symbols don't link-clash with libc's.

## Trying it on a PC

```
cd host_test
build.bat
small-ffs-test.exe
```

`build.bat` finds Visual Studio 2022 (or 2019 Build Tools) automatically.
It compiles `fs.c` + `list.c` against a 2 MiB binary file simulating an
Adesto AT25SF161 with NOR-style write semantics enforced (writes AND into
existing bytes — any 0→1 flip without erase is flagged as a violation).

What's exercised: `Format_fs`, round-trip read/write, `fs_fclip` stress
(16 rounds of random front-clips verified byte-by-byte against an in-RAM
model), and a simulated power-loss scenario (write, drop FS state, run
`fs_scan_media`, read back).

Full details in [host_test/README.md](host_test/README.md).

## Known issues / next steps

See [REVIEW.md](../REVIEW.md) (kept outside the repo) for the review notes.
Short list of things worth addressing in follow-up commits:

1. **Wear-leveling power-loss window.** Erase happens before the new
   `erase_count` is written back to the sector head; a power loss in
   that window loses the count history.
2. **FAT file_num wrap-around.** `fs_find_fat` picks the highest
   `file_num` as the active FAT — wraps catastrophically once the counter
   rolls. Decades away at typical write rates, but silent when it hits.
3. **`fs_fileinfo` macro misuse.** Body at `fs.c:3008` uses `page_data_size`
   (a function-like macro) as a value. Currently gated by `#ifdef FS_FILEINFO`
   so it doesn't compile by default; the host rig supplies a working stand-in.
4. **Dead-page revival is commented out** — pages with a corrupt `page_info`
   leak forever. Not a corruption risk, just a slow capacity bleed.
5. **Stub mutex.** `fs_mutex_take()` / `fs_mutex_give()` are no-ops. On an
   RTOS you must replace them.

## History

* 2009 — original superloop FS for an STM32F10x logger (Hein de Kock).
* ~2017–2019 — MKII rev: embedded the file-handle data buffer (was a
  caller-supplied pointer), expanded the error enum, kept uint16 page refs.
* 2021–2022 — CT9 rev: widened page/file refs to uint32, added the
  sector_info head and quick-start FAT cache, multi-chip auto-detect,
  verify-after-write, and the `fs_t` singleton that this current code
  uses.
