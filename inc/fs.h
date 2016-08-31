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
#define PSEFFS_ERROR_ILEGALE_CHAR       1
#define PSEFFS_ERROR_FILE_NAME_IN_USE   2
#define PSEFFS_ERROR_FILE_NOT_FOUND     3
#define PSEFFS_ERROR_MEDIA_FULL         4
#define PSEFFS_ERROR_MEDIA_ERROR        5
#define PSEFFS_ERROR_FILE_OPEN          6
#define PSEFFS_ERROR_OUT_OF_MEM         7
#define PSEFFS_ERROR_INVALID_SWITCH     8
#define PSEFFS_ERROR_EOF                9
#define PSEFFS_ERROR_NO_PERMISSION      10

#define ERASE_COUNT_KEY                 0xA5
#define ERASE_COUNT_MASK                0xFFFFFF

#define FIRST_FAT_FILE_NUM              0x8000

//-------------------------------------------------------------------------------------
typedef struct file_info
{
  uint16_t      file_num;             // max 1 File / Page, 4096 Pages -> 4096 Files (uint16_t max 65536)
  char          name[MAX_FNAME_SIZE]; // definable len null terminated string
  uint16_t      start_page;           // 4096 Pages (uint16_t max 65536)
  uint16_t      start_offset;         // 528 Bytes / Page (uint16_t max 65536)
  uint32_t      file_size;            // 2162688 - 65536 =  2 097 152 Bytes Max on Chip (2 Meg)
                              // (uint32_t max 4294967296 (+/- 4.2 Gig))
} __attribute__((packed)) file_info_t;

//-------------------------------------------------------------------------------------
typedef struct page_info
{
  uint16_t      file_num;             // max 4096 Files (uint16_t max 65536)
  uint16_t      prev_page;            // max 4096 Pages (uint16_t max 65536)
  uint16_t      next_page;            // max 4096 Pages (uint16_t max 65536)
  uint16_t      link_num;             // max 4096 Pages -> 4096 Links (uint16_t max 65536)
} __attribute__((packed)) page_info_t;

//-------------------------------------------------------------------------------------
typedef struct file_obj
{
  file_info_t   file_header;
  page_info_t   page_header;
  uint16_t      current_page;     // max 4096 Pages (uint16_t max 65536)
  uint16_t      current_offset;   // 528 Bytes / Page (uint16_t max 65536)
  uint32_t      position;
  uint8_t       mode_flag;        // 1 char
  uint8_t       *buffer;          // char pointer to ram buffer
} __attribute__((packed)) FS_FILE;

//-------------------------------------------------------------------------------------
typedef struct fat_obj
{
  page_info_t   page_header;
  uint16_t      start_page;       // 4096 Pages (uint16_t max 65536)
  uint16_t      current_page;     // 4096 Pages (uint16_t max 65536)
  uint16_t      current_offset;   // 512 Data Bytes / Page (uint16_t max 65536)
  uint8_t       *buffer;          // char pointer to ram buffer
} __attribute__((packed)) FAT;

//-------------------------------------------------------------------------------------
#define FAT_EOF                   0xFF

//-------------------------------------------------------------------------------------
void      shutdown_fs               (void);
FS_FILE   *fs_fopen                 (const char *filename, uint8_t mode);
bool      fs_fclose                 (FS_FILE *fp);
void      fs_fcloseall              (void);
bool      fs_fputc                  (int c, FS_FILE *file);
int       fs_fputs                  (const char *buf, FS_FILE *file);
int       fs_fgetc                  (FS_FILE *file);
int       fs_fgets                  (char *buf, int max_chars, FS_FILE *file);
int32_t   fs_access                 (const char *fname);
bool      fs_remove                 (const char *filename);
bool      fs_rename                 (const char *oldfile, const char *newfile);
bool      fs_remove_num             (uint16_t filenum);

uint32_t  fs_fseekend               (FS_FILE *fp);
void      fs_fsetpos                (FS_FILE *fp, uint32_t position);
void      fs_freverse               (FS_FILE *fp, uint32_t numchars);
void      fs_fgetsposn              (char *buf, int num_chars, FS_FILE *file, int start_pos);

bool      fs_fclip                  (const char *filename, uint32_t num_bytes);
void      fs_reset_file             (FS_FILE *fp);
int32_t   read_file_list            (char *name, uint16_t *filenum, uint32_t *filesize);
void      reset_file_list           (void);

void      fs_print_sector           (uint16_t sector);
void      fs_print_fat              (void);
void      fs_print_fatbak           (void);

void      Format_fs                 (void);
void      fs_scan_media             (void);

void      file_printf               (FS_FILE *f, const char *ptr, ...);

void      fs_fileinfo               (uint16_t *num_files, uint32_t *used_size, uint16_t *fat_size, uint32_t *sys_size );

void      Format_Progress_Callback  (uint16_t page_number, uint16_t end_page);

void      scan_fs_callback          (void);

void      fs_error_callback         (const char *err);

//void      fs_test                   (void);

void      scan_start_callback       (uint16_t file_system_start, uint16_t file_system_end);
void      scan_integrity_fail_callback  (uint16_t file_num, char *filename);
void      scan_dead_page_callback   (uint16_t page);
void      scan_file_info_callback   (uint16_t file_num, char *filename, uint16_t page, uint16_t link_num);
void      scan_page_erased_callback (uint16_t);
void      scan_fragment_callback    (uint16_t file_num, uint16_t page);
void      scan_end_callback         (uint16_t dead_pages);

void      watchdog_reset_callback   (void);

//-------------------------------------------------------------------------------------
bool      fs_mutex_take             (void);
void      fs_mutex_give             (void);

//-------------------------------------------------------------------------------------
#endif
//-------------------------------------------------------------------------------------
