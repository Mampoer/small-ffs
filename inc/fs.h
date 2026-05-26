//-------------------------------------------------------------------------------------
// Copyright (c) 2009, Hein de Kock
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this list
//   of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice, this
//   list of conditions and the following disclaimer in the documentation and/or other
//   materials provided with the distribution.
// * Neither the name of the author nor the names of its contributors may be used to
//   endorse or promote products derived from this software without specific prior
//   written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

//-------------------------------------------------------------------------------------
#ifndef _FS_H_
#define _FS_H_

#include "ffs_config.h"

//-------------------------------------------------------------------------------------
typedef enum {
  PSEFFS_ERROR_NONE                     = 0,
  PSEFFS_ERROR_ILEGALE_CHAR             = 1,
  PSEFFS_ERROR_FILE_NAME_IN_USE         = 2,
  PSEFFS_ERROR_FILE_NOT_FOUND           = 3,
  PSEFFS_ERROR_MEDIA_FULL               = 4,
  PSEFFS_ERROR_MEDIA_ERROR              = 5,
  PSEFFS_ERROR_FILE_OPEN                = 6,
  PSEFFS_ERROR_OUT_OF_MEM               = 7,
  PSEFFS_ERROR_INVALID_SWITCH           = 8,
  PSEFFS_ERROR_EOF                      = 9,
  PSEFFS_ERROR_NO_PERMISSION            = 10,
  PSEFFS_ERROR_OS_LOCK                  = 11,
  PSEFFS_ERROR_HANDLE                   = 12,
  PSEFFS_ERROR_NO_NEXT_PAGE             = 13,
} pseffs_error_t;

#define ERASE_COUNT_KEY                 0xA6
#define ERASE_COUNT_MASK                0xFFFFFF

#define FIRST_FAT_FILE_NUM              0x80000000

#define DATA_BUF_SIZE                   128

//-------------------------------------------------------------------------------------
typedef struct file_info
{
  uint32_t      file_num;               // max 1 File / Page, 4096 Pages -> 4096 Files (uint16_t max 65536)
  char          name[MAX_FNAME_SIZE];   // definable len null terminated string
  uint32_t      start_page;             // 4096 Pages (uint16_t max 65536)
  uint32_t      start_offset;           // 528 Bytes / Page (uint16_t max 65536)
  uint32_t      file_size;              // 2162688 - 65536 =  2 097 152 Bytes Max on Chip (2 Meg)
                                        // (uint32_t max 4294967296 (+/- 4.2 Gig))
} __attribute__((packed)) file_info_t;

//-------------------------------------------------------------------------------------
typedef struct page_info
{
  uint32_t      file_num;               // max 4096 Files (uint16_t max 65536)
  uint32_t      prev_page;              // max 4096 Pages (uint16_t max 65536)
  uint32_t      next_page;              // max 4096 Pages (uint16_t max 65536)
  uint32_t      link_num;               // max 4096 Pages -> 4096 Links (uint16_t max 65536)
} __attribute__((packed)) page_info_t;

//-------------------------------------------------------------------------------------
typedef struct __FILE
{
  file_info_t   file_info;
  page_info_t   page_info;
  uint32_t      current_page;             //
  uint32_t      current_offset;           //
  uint32_t      position;
  uint8_t       mode_flag;                // 1 char
  uint8_t       data_buf[DATA_BUF_SIZE];  // char pointer to ram buffer
  uint32_t      data_buf_offset;
  uint32_t      data_buf_index;
} __attribute__((packed)) _FILE;

//-------------------------------------------------------------------------------------
typedef struct fat_obj
{
  page_info_t   page_info;
  uint32_t      start_page;             // 
  uint32_t      current_page;           // 
  uint32_t      current_offset;         // 
} __attribute__((packed)) FAT;

//-------------------------------------------------------------------------------------
#define   FAT_EOF                       0xFF

//-------------------------------------------------------------------------------------
typedef struct fs {
  list_t          *open_file_list           ;
  FAT             current_fat               ;
  uint32_t        next_file_num             ;
  int             open_file_list_size       ;
  //uint8_t         dead_page_revive_count    ;
  uint32_t        start_search_page         ;
  uint32_t        wear_threshold            ;
  uint32_t        init_key                  ;
} fs_t;

//-------------------------------------------------------------------------------------
void      shutdown_fs                   (void);
void      fs_fcloseall                  (fs_t *fs);

int32_t   fs_access                     (const char *fname);

//bool      fs_remove_num                 (uint32_t filenum);

//uint32_t  fs_fseekend                   (FS_FILE *fp);
//void      fs_fsetpos                    (FS_FILE *fp, uint32_t position);
//void      fs_freverse                   (FS_FILE *fp, uint32_t numchars);
//void      fs_fgetsposn                  (char *buf, int num_chars, FS_FILE *file, int start_pos);

bool      fs_fclip                      (const char *filename, uint32_t num_bytes);
//void      fs_reset_file                 (FS_FILE *fp);

int32_t   read_file_list                (char *name, uint32_t *filenum, uint32_t *filesize);
void      reset_file_list               (void);

//void      fs_print_sector               (uint32_t sector);
//void      fs_print_fat                  (void);
//void      fs_print_fatbak               (void);

void      Format_fs                     (void);
void      fs_scan_media                 (void);

void      fs_fileinfo                   (uint32_t *num_files, uint32_t *used_size, uint32_t *fat_size, uint32_t *sys_size );

void      Format_Progress_Callback      (uint32_t page_number, uint32_t end_page);

void      scan_fs_callback              (void);

void      media_error_callback             (const char *err);

//void      fs_test                       (void);

void      scan_callback                 (uint32_t page);
void      scan_start_callback           (uint32_t file_system_start_page, uint32_t file_system_end_page);
void      scan_integrity_fail_callback  (uint32_t file_num, char *filename);
void      scan_dead_page_callback       (uint32_t page);
void      scan_file_info_callback       (uint32_t file_num, char *filename, uint32_t page, uint32_t link_num);
void      scan_page_erased_callback     (uint32_t);
void      scan_fragment_callback        (uint32_t file_num, uint32_t page);
void      scan_end_callback             (uint32_t dead_pages);
void      fat_search_callback           (uint32_t page);
void      fragment_search_callback      (uint32_t file_num, uint32_t page);

void      flash_driver_save_quick_start_fat    (uint32_t fat_page);
uint32_t  flash_driver_load_quick_start_fat    (void);

//-------------------------------------------------------------------------------------
void      pseffs_error_callback         (pseffs_error_t pseffs_error);

//-------------------------------------------------------------------------------------
#define   fs_mutex_take()               1
#define   fs_mutex_give()

//-------------------------------------------------------------------------------------
#define   page_info_size                sizeof (page_info_t)
#define   page_info_address(page)       ((page % flash_pages_per_sector ? 0 : sector_info_size) + (page * flash_page_size))
#define   page_data_address(page)       (page_info_address(page) + page_info_size)
#define   page_data_size(page)          (flash_page_size - page_info_size - (page % flash_pages_per_sector ? 0 : sector_info_size))
#define   page_sector_address(page)     ((page / flash_pages_per_sector) * flash_sector_size)

//-------------------------------------------------------------------------------------
#endif
//-------------------------------------------------------------------------------------
