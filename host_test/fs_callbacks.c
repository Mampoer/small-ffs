//-------------------------------------------------------------------------------------
// Callback stubs that fs.c expects the application to provide. All log to stderr
// so the test runner can see what the FS is doing internally.
//-------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdint.h>

#include "fs.h"

//-------------------------------------------------------------------------------------
// Keil retargeting symbols that fs.c expects when fopen/fputc/fgetc are called
// with stdin/stdout/stderr handles. On the host we never use those, so stubs
// are fine.
//-------------------------------------------------------------------------------------
int stdin_getchar (void) { return -1; }
int stdout_putchar (int c) { (void)c; return c; }

//-------------------------------------------------------------------------------------
// fs_fileinfo is declared in fs.h but its body in fs.c is wrapped in
// `#ifdef FS_FILEINFO` AND has a latent bug at line 3008 (uses `page_data_size`
// as a value but it's a function-like macro). Provide a tiny working stub so
// callers get sensible numbers — set everything to zero except sys_size which
// is computed from the runtime flash params.
//-------------------------------------------------------------------------------------
extern uint32_t flash_page_size;
extern uint32_t file_system_start_page;
extern uint32_t file_system_end_page;

void fs_fileinfo (uint32_t *num_files, uint32_t *used_size, uint32_t *fat_size, uint32_t *sys_size)
{
  if (num_files) *num_files = 0;
  if (used_size) *used_size = 0;
  if (fat_size)  *fat_size  = 0;
  if (sys_size)  *sys_size  = (file_system_end_page - file_system_start_page + 1) * flash_page_size;
}

//-------------------------------------------------------------------------------------
// Verbosity control — set from test_main to silence scan chatter during stress
// tests.
//-------------------------------------------------------------------------------------
int g_verbose = 0;

#define VLOG(...) do { if (g_verbose) fprintf(stderr, "  [cb] " __VA_ARGS__); } while (0)

//-------------------------------------------------------------------------------------
void Format_Progress_Callback (uint32_t page_number, uint32_t end_page)
{
  if (g_verbose && (page_number % 256 == 0 || page_number == end_page))
    fprintf (stderr, "  [format] page %u/%u\n", page_number, end_page);
}

//-------------------------------------------------------------------------------------
void scan_fs_callback (void)
{
  VLOG ("scan_fs_callback (media-error recovery)\n");
}

//-------------------------------------------------------------------------------------
void media_error_callback (const char *err)
{
  fprintf (stderr, "  [media_error] %s\n", err ? err : "(null)");
}

//-------------------------------------------------------------------------------------
void scan_callback                (uint32_t page) { (void)page; }
void scan_start_callback          (uint32_t s, uint32_t e) { VLOG ("scan start [%u..%u]\n", s, e); }
void scan_integrity_fail_callback (uint32_t n, char *name)  { fprintf (stderr, "  [integrity_fail] file %u %s\n", n, name); }
void scan_dead_page_callback      (uint32_t page) { VLOG ("dead page %u\n", page); }
void scan_file_info_callback      (uint32_t n, char *name, uint32_t page, uint32_t link) { VLOG ("file %u %s page %u link %u\n", n, name, page, link); }
void scan_page_erased_callback    (uint32_t page) { (void)page; }
void scan_fragment_callback       (uint32_t n, uint32_t page) { VLOG ("fragment file %u page %u\n", n, page); }
void scan_end_callback            (uint32_t dead) { VLOG ("scan end — %u dead pages\n", dead); }
void fat_search_callback          (uint32_t page) { (void)page; }
void fragment_search_callback     (uint32_t n, uint32_t page) { (void)n; (void)page; }

//-------------------------------------------------------------------------------------
void pseffs_error_callback (pseffs_error_t err)
{
  static const char *names[] = {
    "NONE", "ILEGALE_CHAR", "FILE_NAME_IN_USE", "FILE_NOT_FOUND",
    "MEDIA_FULL", "MEDIA_ERROR", "FILE_OPEN", "OUT_OF_MEM",
    "INVALID_SWITCH", "EOF", "NO_PERMISSION", "OS_LOCK", "HANDLE",
    "NO_NEXT_PAGE"
  };
  const char *n = (err < sizeof(names)/sizeof(names[0])) ? names[err] : "?";
  if (err != PSEFFS_ERROR_NONE && err != PSEFFS_ERROR_EOF)
    fprintf (stderr, "  [fs_err] %s (%d)\n", n, (int)err);
}
