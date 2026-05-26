//-------------------------------------------------------------------------------------
// Minimal host-side declarations for the FS, usable from test_main.c WITHOUT
// pulling in fs.h (which would clash with libc's <stdio.h> FILE).
//-------------------------------------------------------------------------------------
#ifndef _SMALL_FFS_HOST_API_H_
#define _SMALL_FFS_HOST_API_H_

#include <stdint.h>
#include <stdbool.h>

// Opaque file handle.
struct __FILE;
typedef struct __FILE FsFile;

// Stdio-overlay entry points (renamed via host_shim.h when fs.c was built).
FsFile *fs_fopen   (const char *filename, const char *mode);
int     fs_fclose  (FsFile *fp);
int     fs_fputc   (int c, FsFile *fp);
int     fs_fgetc   (FsFile *fp);
int     fs_fputs   (const char *s, FsFile *fp);
int     fs_remove  (const char *filename);

// FS public surface (not renamed by host_shim.h).
int32_t fs_access     (const char *fname);
bool    fs_fclip      (const char *filename, uint32_t num_bytes);
void    Format_fs     (void);
void    fs_scan_media (void);
void    fs_fileinfo   (uint32_t *num_files, uint32_t *used, uint32_t *fat, uint32_t *sys);

// Flash driver.
bool    flash_driver_init  (void);

#endif
