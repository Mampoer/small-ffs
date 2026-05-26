//-------------------------------------------------------------------------------------
// Host test runner for small-ffs. Exercises format, round-trip, fs_fclip stress
// and simulated power-loss + rescan. Uses the file-backed flash mock.
//-------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "fs_host_api.h"

extern int g_verbose;
extern void flash_mock_set_path (const char *path);
extern void flash_mock_stats (uint64_t *writes, uint64_t *reads, uint64_t *erases, uint8_t *violations);

//-------------------------------------------------------------------------------------
static int    g_failed = 0;
static int    g_passed = 0;

#define CHECK(cond, fmt, ...) do {                                          \
  if (!(cond)) {                                                            \
    fprintf (stderr, "FAIL %s:%d  " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    g_failed++;                                                             \
  } else {                                                                  \
    g_passed++;                                                             \
  }                                                                         \
} while (0)

//-------------------------------------------------------------------------------------
static void hdr (const char *title)
{
  fprintf (stderr, "\n=== %s ===\n", title);
}

//-------------------------------------------------------------------------------------
static uint32_t file_size_or_zero (const char *name)
{
  int32_t sz = fs_access (name);
  return sz < 0 ? 0 : (uint32_t)sz;
}

//-------------------------------------------------------------------------------------
static void test_format (void)
{
  hdr ("format");
  Format_fs ();
  uint32_t num_files=0, used=0, fat=0, sys=0;
  fs_fileinfo (&num_files, &used, &fat, &sys);
  CHECK (num_files == 0, "expected 0 files, got %u", num_files);
  CHECK (used == 0, "expected used==0, got %u", used);
  fprintf (stderr, "  format ok — fat_size=%u sys_size=%u\n", fat, sys);
}

//-------------------------------------------------------------------------------------
static void test_round_trip (void)
{
  hdr ("round trip");
  FsFile *fp = fs_fopen ("app.log", "w");
  CHECK (fp != NULL, "fopen for write returned NULL");
  if (!fp) return;

  const int N = 1024;
  for (int i = 0; i < N; i++) fs_fputc ((uint8_t)(i & 0xFF), fp);
  fs_fclose (fp);

  CHECK (file_size_or_zero ("app.log") == (uint32_t)N, "size after write");

  fp = fs_fopen ("app.log", "r");
  CHECK (fp != NULL, "fopen for read returned NULL");
  if (!fp) return;

  int mismatches = 0;
  for (int i = 0; i < N; i++)
  {
    int c = fs_fgetc (fp);
    if (c != (i & 0xFF)) mismatches++;
  }
  fs_fclose (fp);
  CHECK (mismatches == 0, "%d mismatches in %d bytes", mismatches, N);
  fprintf (stderr, "  round trip ok — %d bytes\n", N);
}

//-------------------------------------------------------------------------------------
// fs_fclip stress — append a known stream, clip random amounts off the front,
// verify size and remaining-prefix.
//-------------------------------------------------------------------------------------
static void test_fclip_stress (void)
{
  hdr ("fs_fclip stress");
  fs_remove ("trim.log");

  FsFile *fp = fs_fopen ("trim.log", "w");
  CHECK (fp != NULL, "fopen trim.log");
  if (!fp) return;

  const uint32_t N = 8192;
  for (uint32_t i = 0; i < N; i++) fs_fputc ((uint8_t)(i * 31 + 7), fp);
  fs_fclose (fp);
  CHECK (file_size_or_zero ("trim.log") == N, "initial size");

  uint32_t logical_head = 0;
  uint32_t remaining    = N;

  srand (42);
  for (int round = 0; round < 16 && remaining > 64; round++)
  {
    uint32_t clip = (uint32_t)(rand () % (remaining / 4 + 1));
    if (clip == 0) clip = 1;
    bool ok = fs_fclip ("trim.log", clip);
    CHECK (ok, "fs_fclip returned false on round %d clip=%u", round, clip);

    logical_head += clip;
    remaining    -= clip;

    uint32_t sz = file_size_or_zero ("trim.log");
    if (sz != remaining)
    {
      fprintf (stderr, "  round %d: clip=%u expected size=%u got=%u  (head=%u)\n",
               round, clip, remaining, sz, logical_head);
      g_failed++;
      return;
    }

    fp = fs_fopen ("trim.log", "r");
    CHECK (fp != NULL, "reopen after clip");
    if (!fp) return;

    int mismatches = 0;
    for (uint32_t i = 0; i < remaining; i++)
    {
      int c = fs_fgetc (fp);
      uint8_t exp = (uint8_t)((logical_head + i) * 31 + 7);
      if (c != exp) { mismatches++; if (mismatches < 4) fprintf (stderr, "  byte %u: got 0x%02x expected 0x%02x\n", i, c, exp); }
    }
    fs_fclose (fp);
    CHECK (mismatches == 0, "%d byte mismatches after clip %d (remaining %u)",
           mismatches, round, remaining);

    fprintf (stderr, "  round %2d: clipped %4u, remaining %5u — ok\n",
             round, clip, remaining);
  }
}

//-------------------------------------------------------------------------------------
// Simulated power loss: write a file, do NOT call fs_fcloseall, drop the in-RAM
// FS state (calling fs_scan_media on its own forces a fresh discovery from
// flash), then read the file back.
//-------------------------------------------------------------------------------------
static void test_power_loss (void)
{
  hdr ("power loss + rescan");

  const char *name = "persist.log";
  fs_remove (name);
  FsFile *fp = fs_fopen (name, "w");
  CHECK (fp != NULL, "fopen persist.log");
  if (!fp) return;
  const char *msg = "the quick brown fox jumps over the lazy dog 0123456789";
  fs_fputs (msg, fp);
  fs_fclose (fp);

  fprintf (stderr, "  forcing rescan...\n");
  fs_scan_media ();

  uint32_t sz = file_size_or_zero (name);
  CHECK (sz == (uint32_t)strlen (msg), "size after rescan: expected %zu got %u",
         strlen (msg), sz);

  fp = fs_fopen (name, "r");
  CHECK (fp != NULL, "reopen after rescan");
  if (!fp) return;

  char buf[128] = {0};
  for (size_t i = 0; i < strlen (msg); i++) buf[i] = (char)fs_fgetc (fp);
  fs_fclose (fp);

  CHECK (memcmp (buf, msg, strlen (msg)) == 0, "payload mismatch after rescan");
  fprintf (stderr, "  power loss test ok\n");
}

//-------------------------------------------------------------------------------------
int main (int argc, char **argv)
{
  const char *path = argc > 1 ? argv[1] : "flash.bin";
  bool fresh = false;

  for (int i = 1; i < argc; i++)
  {
    if (!strcmp (argv[i], "-v")) g_verbose = 1;
    else if (!strcmp (argv[i], "--fresh")) fresh = true;
    else if (argv[i][0] != '-') path = argv[i];
  }

  if (fresh) remove (path);  // host libc remove, NOT FS — backing file
  flash_mock_set_path (path);

  if (!flash_driver_init ())
  {
    fprintf (stderr, "flash_driver_init failed\n");
    return 1;
  }

  // First boot: try to find a valid FAT; if none, format.
  fs_scan_media ();
  uint32_t nf=0, used=0, fat=0, sys=0;
  fs_fileinfo (&nf, &used, &fat, &sys);
  if (fat == 0)
  {
    fprintf (stderr, "no FAT detected — formatting fresh\n");
    Format_fs ();
  }

  test_format ();
  test_round_trip ();
  test_fclip_stress ();
  test_power_loss ();

  uint64_t writes, reads, erases; uint8_t viol;
  flash_mock_stats (&writes, &reads, &erases, &viol);

  fprintf (stderr, "\n=== summary ===\n");
  fprintf (stderr, "  passed:     %d\n", g_passed);
  fprintf (stderr, "  failed:     %d\n", g_failed);
  fprintf (stderr, "  flash IO:   %llu B written, %llu B read, %llu sectors erased\n",
           (unsigned long long)writes, (unsigned long long)reads, (unsigned long long)erases);
  fprintf (stderr, "  nor viol:   %u\n", viol);

  return g_failed == 0 ? 0 : 1;
}
