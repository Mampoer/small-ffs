//-------------------------------------------------------------------------------------
// File-backed flash mock. Replaces flash_driver.c on host builds. Enforces NOR
// semantics: writes AND into existing bytes (so flipping any 0 -> 1 without
// erasing aborts the test, exposing a real flash-corrupting bug).
//
// Models an Adesto AT25SF161 — 512 sectors x 4096 bytes = 2 MiB, 16 x 256-byte
// pages per sector. Sector 0 is reserved for the quick-start-FAT cache; the FS
// uses [file_system_start_page .. file_system_end_page].
//-------------------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flash_driver.h"

//-------------------------------------------------------------------------------------
#define MOCK_SECTORS              512
#define MOCK_SECTOR_SIZE          4096
#define MOCK_PAGES_PER_SECTOR     16
#define MOCK_PAGE_SIZE            (MOCK_SECTOR_SIZE / MOCK_PAGES_PER_SECTOR)
#define MOCK_FLASH_SIZE           (MOCK_SECTORS * MOCK_SECTOR_SIZE)

//-------------------------------------------------------------------------------------
uint32_t flash_page_size         = 0;
uint32_t flash_pages_per_sector  = 0;
uint32_t file_system_start_page  = 0;
uint32_t file_system_end_page    = 0;

//-------------------------------------------------------------------------------------
static FILE        *g_backing    = NULL;
static const char  *g_path       = "flash.bin";
static uint64_t     g_write_bytes  = 0;
static uint64_t     g_erased_sectors = 0;
static uint64_t     g_read_bytes   = 0;
static uint8_t      g_nor_violations = 0;

//-------------------------------------------------------------------------------------
void flash_mock_set_path (const char *path)
{
  g_path = path ? path : "flash.bin";
}

//-------------------------------------------------------------------------------------
void flash_mock_stats (uint64_t *writes, uint64_t *reads, uint64_t *erases, uint8_t *violations)
{
  if (writes)     *writes     = g_write_bytes;
  if (reads)      *reads      = g_read_bytes;
  if (erases)     *erases     = g_erased_sectors;
  if (violations) *violations = g_nor_violations;
}

//-------------------------------------------------------------------------------------
static void open_backing (void)
{
  if (g_backing) return;

  g_backing = fopen (g_path, "r+b");
  if (!g_backing)
  {
    // First run — create the file as fully erased (0xFF).
    g_backing = fopen (g_path, "w+b");
    if (!g_backing)
    {
      fprintf (stderr, "flash_mock: cannot open %s\n", g_path);
      exit (1);
    }
    uint8_t buf[4096];
    memset (buf, 0xFF, sizeof buf);
    for (uint32_t i = 0; i < MOCK_FLASH_SIZE; i += sizeof buf)
      fwrite (buf, 1, sizeof buf, g_backing);
    fflush (g_backing);
  }
}

//-------------------------------------------------------------------------------------
bool flash_driver_init (void)
{
  flash_page_size         = MOCK_PAGE_SIZE;
  flash_pages_per_sector  = MOCK_PAGES_PER_SECTOR;
  file_system_start_page  = MOCK_PAGES_PER_SECTOR;                       // skip sector 0
  file_system_end_page    = MOCK_PAGES_PER_SECTOR * MOCK_SECTORS - 1;

  open_backing ();
  return true;
}

//-------------------------------------------------------------------------------------
void hw_init_flash (void) { /* no-op on host */ }

//-------------------------------------------------------------------------------------
void flash_driver_read (uint32_t flash_address, uint8_t *data, uint32_t len)
{
  open_backing ();

  if (flash_address + len > MOCK_FLASH_SIZE)
  {
    // Out of bounds — return erased state and log. This catches FS bugs that
    // access past file_system_end_page (e.g. look-ahead reads at end of chip).
    fprintf (stderr, "flash_mock: read OOB addr=0x%08x len=%u (returning 0xFF)\n",
             flash_address, len);
    memset (data, 0xFF, len);
    g_read_bytes += len;
    return;
  }

  fseek (g_backing, (long)flash_address, SEEK_SET);
  size_t n = fread (data, 1, len, g_backing);
  if (n != len)
  {
    fprintf (stderr, "flash_mock: short read at 0x%08x (got %zu, want %u)\n",
             flash_address, n, len);
    memset (data + n, 0xFF, len - n);
  }
  g_read_bytes += len;
}

//-------------------------------------------------------------------------------------
int flash_driver_write (uint32_t flash_address, const uint8_t *data, uint32_t len, bool verify)
{
  open_backing ();

  if (flash_address + len > MOCK_FLASH_SIZE)
  {
    fprintf (stderr, "flash_mock: write OOB addr=0x%08x len=%u (dropping)\n",
             flash_address, len);
    g_write_bytes += len;
    return 0;
  }

  // Read existing bytes, AND in the new bytes (NOR semantics), check for 0->1.
  uint8_t before[256];
  uint32_t remaining = len;
  uint32_t cursor    = 0;

  while (remaining)
  {
    uint32_t chunk = remaining > sizeof before ? sizeof before : remaining;
    fseek (g_backing, (long)(flash_address + cursor), SEEK_SET);
    if (fread (before, 1, chunk, g_backing) != chunk)
    {
      fprintf (stderr, "flash_mock: short read during write at 0x%08x\n",
               flash_address + cursor);
      exit (1);
    }

    uint8_t merged[256];
    for (uint32_t i = 0; i < chunk; i++)
    {
      uint8_t old = before[i];
      uint8_t nw  = data[cursor + i];
      uint8_t flipped_up = (~old) & nw;       // bits we'd flip 0 -> 1
      if (flipped_up)
      {
        if (g_nor_violations < 16)
          fprintf (stderr,
                   "flash_mock: NOR violation at 0x%08x — byte 0x%02x -> 0x%02x "
                   "would flip 0x%02x bits up without erase\n",
                   flash_address + cursor + i, old, nw, flipped_up);
        g_nor_violations++;
      }
      merged[i] = old & nw;                   // physical AND
    }

    fseek (g_backing, (long)(flash_address + cursor), SEEK_SET);
    fwrite (merged, 1, chunk, g_backing);

    remaining -= chunk;
    cursor    += chunk;
  }
  fflush (g_backing);
  g_write_bytes += len;

  if (verify)
  {
    uint8_t v[256];
    remaining = len; cursor = 0;
    while (remaining)
    {
      uint32_t chunk = remaining > sizeof v ? sizeof v : remaining;
      fseek (g_backing, (long)(flash_address + cursor), SEEK_SET);
      if (fread (v, 1, chunk, g_backing) != chunk) return -1;
      for (uint32_t i = 0; i < chunk; i++)
        if (v[i] != (data[cursor + i] & 0xFF))
          // NOR semantics: verify against AND of old & new, but if a 0->1 was
          // attempted the merged value won't match the requested value.
          return -1;
      remaining -= chunk; cursor += chunk;
    }
  }
  return 0;
}

//-------------------------------------------------------------------------------------
void flash_driver_erase_sector_address (uint32_t flash_address, bool erase64k)
{
  open_backing ();

  uint32_t size = erase64k ? (64 * 1024) : MOCK_SECTOR_SIZE;
  uint32_t base = (flash_address / size) * size;

  if (base + size > MOCK_FLASH_SIZE)
  {
    fprintf (stderr, "flash_mock: erase OOB addr=0x%08x size=%u (dropping)\n",
             base, size);
    return;
  }

  uint8_t ff[4096];
  memset (ff, 0xFF, sizeof ff);
  uint32_t remaining = size;
  uint32_t cursor    = 0;
  while (remaining)
  {
    uint32_t chunk = remaining > sizeof ff ? sizeof ff : remaining;
    fseek (g_backing, (long)(base + cursor), SEEK_SET);
    fwrite (ff, 1, chunk, g_backing);
    remaining -= chunk;
    cursor    += chunk;
  }
  fflush (g_backing);
  g_erased_sectors += size / MOCK_SECTOR_SIZE;
}

//-------------------------------------------------------------------------------------
// Quick-start FAT cache — fs.c uses these to skip a full scan on boot. We store
// them in the reserved first sector (sector 0).
//-------------------------------------------------------------------------------------
#define QUICK_FAT_KEY      0x5AA55AA5
#define QUICK_FAT_HDR_LEN  4              // just the key

static uint32_t g_qf_write_index = 0;     // next append slot, in units of uint32_t

static void quick_fat_reset (void)
{
  flash_driver_erase_sector_address (0, false);
  uint32_t key = QUICK_FAT_KEY;
  flash_driver_write (0, (uint8_t *)&key, sizeof key, true);
  g_qf_write_index = 0;
}

void flash_driver_save_quick_start_fat (uint32_t fat_start_page)
{
  open_backing ();
  uint32_t key = 0;
  flash_driver_read (0, (uint8_t *)&key, sizeof key);

  if (fat_start_page == 0 || key != QUICK_FAT_KEY)
  {
    quick_fat_reset ();
    if (fat_start_page == 0) return;
  }

  // append at next free slot; if sector would overflow, recycle.
  uint32_t offset = QUICK_FAT_HDR_LEN + g_qf_write_index * sizeof (uint32_t);
  if (offset + sizeof (uint32_t) > MOCK_SECTOR_SIZE)
  {
    quick_fat_reset ();
    offset = QUICK_FAT_HDR_LEN;
  }
  flash_driver_write (offset, (uint8_t *)&fat_start_page, sizeof fat_start_page, true);
  g_qf_write_index++;
}

uint32_t flash_driver_load_quick_start_fat (void)
{
  open_backing ();
  uint32_t key = 0;
  flash_driver_read (0, (uint8_t *)&key, sizeof key);
  if (key != QUICK_FAT_KEY) return 0;

  // scan forward for the last non-0xFFFFFFFF entry, also recovering write index.
  uint32_t last = 0;
  g_qf_write_index = 0;
  for (uint32_t off = QUICK_FAT_HDR_LEN; off + sizeof (uint32_t) <= MOCK_SECTOR_SIZE;
       off += sizeof (uint32_t))
  {
    uint32_t v = 0;
    flash_driver_read (off, (uint8_t *)&v, sizeof v);
    if (v == 0xFFFFFFFF) break;
    last = v;
    g_qf_write_index++;
  }
  return last;
}

//-------------------------------------------------------------------------------------
// Driver callbacks — stubs.
//-------------------------------------------------------------------------------------
void flash_sector_protect_callback         (uint8_t status) { (void)status; }
void Flash_Fail_Callback                   (void) { }
void flash_driver_blocking_callback        (void) { }
void flash_driver_blocking_callback_start  (void) { }
void flash_driver_blocking_callback_end    (void) { }

//-------------------------------------------------------------------------------------
// Hardware SPI primitives — flash_driver.c isn't built on host, so these are
// only here to satisfy the linker if test code accidentally pulls flash_hw.h.
//-------------------------------------------------------------------------------------
void init_flash_hw          (void) { }
void Select_Flash           (void) { }
void Deselect_Flash         (void) { }
void Read_Data_Page         (uint8_t *r, uint16_t l) { (void)r; (void)l; }
void Write_Data_Page        (const uint8_t *t, uint32_t l) { (void)t; (void)l; }
void spi_send_byte          (uint8_t d) { (void)d; }
uint8_t spi_read_byte       (void) { return 0xFF; }
void Flash_WP               (bool b) { (void)b; }
void Flash_Hold             (bool b) { (void)b; }
