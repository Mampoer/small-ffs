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
// 5 December 2019
//-------------------------------------------------------------------------------------
// change log
// 19/12/05: fgetc fs_advance_file error -1 added to handle incorrrect fat file size
//-------------------------------------------------------------------------------------
// bugs: 
//-------------------------------------------------------------------------------------
// todo: 
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
#include "fs.h"
#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//-------------------------------------------------------------------------------------
void DEBUG_printf (int level, char *file, int line, const char *ptr, ...);

//-------------------------------------------------------------------------------------
#define debug_printf(...) //{DEBUG_printf (0, "FS", __LINE__, __VA_ARGS__);}

//-------------------------------------------------------------------------------------
#define fs_printf(...)                      //term_printf(__VA_ARGS__)
#define debug_printf_file_header(x);
#define debug_printf_page_header(x,y,z);

//-------------------------------------------------------------------------------------
//#include "uart.h"
//#include "proto.h"
//#define debug_printf(x) DEBUG_printf x 

  FILE __stdout;
  FILE __stdin;
  FILE __stderr;


//-----------------------------------------------------------------------------------
// Debug functions
//-----------------------------------------------------------------------------------
//static void debug_printf_file_info (file_info_t *info)
//{
//  debug_printf ("file_num: %u, name: %s, start_page: %u, start_offset: %u, file_size: %u\n\r",
//  info->file_num, info->name, info->start_page, info->start_offset, info->file_size));
//}

////-----------------------------------------------------------------------------------
//static void debug_printf_page_info (uint32_t page, const page_info_t *info, char *filename)
//{
//  debug_printf ("page: %4u, file_num: %4hX, prev_page: %4u, next_page: %4u, link_num: %4hu, %s\n\r",
//  page, info->file_num, info->prev_page, info->next_page, info->link_num, filename ));
//}

////-------------------------------------------------------------------------------------
//list_t          *open_file_list           __attribute__((section(".init2"))) = NULL;

////-------------------------------------------------------------------------------------
//FAT             current_fat               __attribute__((section(".init2"))) = { {0,0,0,0}, 0, 0, 0 };
//FILE            *putchar_f                __attribute__((section(".init2"))) = NULL;
//uint32_t        next_file_num             __attribute__((section(".init2"))) = 1;
//int             open_file_list_size       __attribute__((section(".init2"))) = 0;
////uint8_t         dead_page_revive_count    = 0;
//uint32_t        start_search_page         __attribute__((section(".init2"))) = 0;
//uint32_t        wear_threshold            __attribute__((section(".init2"))) = WEAR_THRESHOLD;

FILE            *putchar_f                = NULL;

//-------------------------------------------------------------------------------------
#define FLASH_OBJ_INIT_KEY  0xAA5A

//fs_t flash_fs_obj ZERO_INIT = {
//                      .open_file_list           = NULL,
//                      .current_fat              = { {0,0,0,0}, 0, 0, 0 },
//                      .next_file_num            = 1,
//                      .open_file_list_size      = 0,
//                      //.dead_page_revive_count   = 0,
//                      .start_search_page        = 0,
//                      .wear_threshold           = WEAR_THRESHOLD,
//                      .init_key                 = FLASH_OBJ_INIT_KEY,
//                    };

//-------------------------------------------------------------------------------------
// FILE FUNCTIONS
//-------------------------------------------------------------------------------------
static FILE     *fs_get_file_mem          (void);
static uint32_t fs_find_open_page         (void);
static void     fs_load_file              (FILE *fp);
static bool     fs_advance_file           (FILE *fp);
static void     fs_free_file_mem          (FILE *fp);
static bool     fp_valid                  (FILE *fp);
static void     fs_remove_file_chain      (uint32_t page);
static void     fs_remove_file_fragments  (uint32_t file_num);
static bool     fs_file_is_open           (char *fname);
static bool     fs_format_fname           (char *fname_buf, const char *fname);
static bool     fs_init_new_file          (char *uppercase_fname, FILE *fp);
static bool     fs_find_file_info         (char *uppercase_fname, FILE *fp);
static bool     fs_find_file_num          (uint32_t filenum, FILE *fp);
static void     fs_free_page              (uint32_t page);
static bool     fs_advance_info           (FILE *fp);
//static bool     fs_advance_page           (FILE *fp);
static bool     fs_reverse_info           (FILE *fp);
//static bool     fs_reverse_page           (FILE *fp);

//static uint32_t rebuild_page_chain        (uint32_t end_page);

//-------------------------------------------------------------------------------------
// FAT FUNCTIONS
//-------------------------------------------------------------------------------------
static bool     fs_find_fat               (FAT *fat);
static bool     fs_create_fat             (FAT *fat, uint32_t fat_file_num);
static void     fs_reset_fat              (void);
static bool     fs_fat_remove_num         (uint32_t file_num);
static void     fs_fat_add                (file_info_t *new_fat_entry);
static bool     fs_fat_get_next_entry     (FAT *fat, file_info_t *entry);
static bool     fs_fat_put_next_entry     (FAT *fat, file_info_t *entry, char *fat_name);

////-------------------------------------------------------------------------------------
//char fs_toupper(char c)
//{
//  if ( c >= 'a' && c <= 'z' )
//    return c - 0x20;
//  
//  return c;
//}

////-------------------------------------------------------------------------------------
//bool fs_isprint(char c)
//{
//  if ( c >= ' ' && c <= '~' )
//    return true;
//  
//  return false;
//}

//-------------------------------------------------------------------------------------
static void read_page_info (uint32_t page_num, void *info) __attribute__((__nonnull__(2)));
static void write_page_info (uint32_t page_num, void *info) __attribute__((__nonnull__(2)));
static void read_page_data (uint32_t page_num, uint32_t offset, void *dest, uint32_t size) __attribute__((__nonnull__(3)));
static void write_page_data (uint32_t page_num, uint32_t offset, void *data, uint32_t size) __attribute__((__nonnull__(3)));
static void read_fat_entry (uint32_t page_num, uint32_t offset, void *entry) __attribute__((__nonnull__(3)));
static void write_fat_entry (uint32_t page_num, uint32_t offset, void *entry) __attribute__((__nonnull__(3)));

//-------------------------------------------------------------------------------------
static void read_page_sector_head (uint32_t page_num, void *head)  __attribute__((__nonnull__(2)));
static void write_page_sector_head (uint32_t page_num, void *head)  __attribute__((__nonnull__(2)));

//-------------------------------------------------------------------------------------
void read_page_info (uint32_t page_num, void *info)
{
  flash_driver_read (page_info_address (page_num), info, page_info_size);
}

//-------------------------------------------------------------------------------------
void write_page_info (uint32_t page_num, void *info)
{
  flash_driver_write (page_info_address (page_num), info, page_info_size, true);
}
  
//-------------------------------------------------------------------------------------
void read_page_data (uint32_t page_num, uint32_t offset, void *data, uint32_t size)
{
  int read_size = size;
  
  if ((offset + read_size ) > page_data_size (page_num))
    read_size = page_data_size (page_num) - offset;
  
  if (size)
  {
    flash_driver_read (page_data_address (page_num) + offset, data, read_size);
  }
}

//-------------------------------------------------------------------------------------
void write_page_data (uint32_t page_num, uint32_t offset, void *data, uint32_t size)
{
  uint32_t write_size = size;
  
  if ((offset + write_size) > page_data_size (page_num))
    write_size = page_data_size (page_num) - offset;
  
  if (size)
  {
    flash_driver_write (page_data_address (page_num) + offset, data, size, true);
  }
}

//-------------------------------------------------------------------------------------
void read_fat_entry (uint32_t page_num, uint32_t offset, void *entry)
{
  if ((offset + sizeof (file_info_t)) <= page_data_size (page_num))
  {
    flash_driver_read (page_data_address (page_num) + offset, entry, sizeof (file_info_t));
  }
}

//-------------------------------------------------------------------------------------
void write_fat_entry (uint32_t page_num, uint32_t offset, void *entry)
{
  if ((offset + sizeof (file_info_t)) <= page_data_size (page_num))
  {
    flash_driver_write (page_data_address (page_num) + offset, entry, sizeof (file_info_t), true);
  }
}

//-------------------------------------------------------------------------------------
void read_page_sector_head (uint32_t page_num, void *head)
{
  flash_driver_read (page_sector_address (page_num), head, sector_info_size);
}

//-------------------------------------------------------------------------------------
void write_page_sector_head (uint32_t page_num, void *head)
{
  flash_driver_write (page_sector_address (page_num), head, sector_info_size, true);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
// File Systems Functions
//-------------------------------------------------------------------------------------
////-------------------------------------------------------------------------------------
//void shutdown_fs(void)
//{
//  int i;

//  debug_printf ("shutdown_fs\n\r");
//  fs_fcloseall();

//  for ( i = 0; i < (2+MAX_OPEN_FILES); i++ )
//  {
//    if ( file_buffer_p[i] ) free(file_buffer_p[i]);
//  }
//}

//-------------------------------------------------------------------------------------
FILE *fopen (const char *filename, const char *mode)
{
  FILE *fp = NULL;
    
  if (fs_mutex_take ())
  {
    char uppercase_fname [MAX_FNAME_SIZE];

    //debug_printf ("fopen %s in mode %c\n\r", filename, mode));  //tvb

    fp = fs_get_file_mem();

    if ( fp )
    {
      if (fs_format_fname (uppercase_fname, filename))
      {
        //debug_printf ("uppercase filename %s \n\r ",uppercase_fname));  //tvb

        if (!fs_file_is_open (uppercase_fname))
        {
          if (fs_find_file_info (uppercase_fname, fp))
          {
            fp->mode_flag = fs_toupper(mode[0]);

            switch (fp->mode_flag)
            {
              case 'R':
                fs_load_file        (fp);
                break;
              case 'W': // this done to save space - the over written file is removed first
                if (fs_fat_remove_num (fp->file_info.file_num)) // clear current fat entry by file num
                {
                  fs_remove_file_chain (fp->file_info.start_page);
                  if (fs_init_new_file (uppercase_fname,fp))
                  {
                    break;
                  }
                }
    
                pseffs_error_callback (PSEFFS_ERROR_MEDIA_ERROR);
                fs_free_file_mem (fp);
                fp = NULL;
                scan_fs_callback ();
                break;
              case 'A':
                fp->current_page = fp->file_info.start_page;  // reset file to start page & offset
                fp->current_offset = fp->file_info.start_offset;
                fp->position = 0;
              
                read_page_info (fp->current_page, &fp->page_info);
              
                while (fp->position < fp->file_info.file_size)
                {
                  int diff = fp->file_info.file_size - fp->position;
                  
                  if ((fp->current_offset + diff) > page_data_size (fp->current_page))
                  {
                    int page_size = page_data_size (fp->current_page);
                    if (fs_advance_info (fp))
                    {
                      fp->position += (page_size - fp->current_offset);
                      fp->current_offset = 0;
                    }
                    else
                      break; // from while
                  }
                  else
                  {
                    fp->current_offset += diff;
                    fp->position += diff;
                  }
                }
                    
                fp->data_buf_offset = fp->current_offset;
                fp->data_buf_index = 0;
              
                memset (fp->data_buf, 0xFF, DATA_BUF_SIZE);
                
                // will start writeing chunk from here
                
                break;
              default:
                pseffs_error_callback (PSEFFS_ERROR_INVALID_SWITCH);
                fs_free_file_mem      (fp);
                break;
            }
          }
          else // file not found
          {
            fp->mode_flag = fs_toupper (mode[0]);
    
            switch (fp->mode_flag)
            {
              case 'W':
              case 'A':
                if (!fs_init_new_file (uppercase_fname,fp))
                {
                  pseffs_error_callback (PSEFFS_ERROR_MEDIA_FULL);
                  fs_free_file_mem (fp);
                  fp = NULL;
                }
                break;
              case 'R':
                debug_printf ("fsopen PSEFFS_ERROR_FILE_NOT_FOUND %s\n\r", filename);
                pseffs_error_callback (PSEFFS_ERROR_FILE_NOT_FOUND);
                fs_free_file_mem (fp);
                fp = NULL;
                break;
              default:
                pseffs_error_callback (PSEFFS_ERROR_INVALID_SWITCH);
                fs_free_file_mem (fp);
                fp = NULL;
                break;
            }
          }
        }
        else
        {
          pseffs_error_callback (PSEFFS_ERROR_FILE_OPEN);
          fs_free_file_mem (fp);
          fp = NULL;
        }
      }
      else
      {
        pseffs_error_callback (PSEFFS_ERROR_ILEGALE_CHAR);
        fs_free_file_mem (fp);
        fp = NULL;
      }
    }
    else
    {
      pseffs_error_callback (PSEFFS_ERROR_OUT_OF_MEM);
    }

    fs_mutex_give ();
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }
      
  return (fp);
}

//-------------------------------------------------------------------------------------
int stdin_getchar (void);

//-------------------------------------------------------------------------------------
int fgetc (FILE *fp)
{
  if (fp == &__stdin)
    return stdin_getchar();

  int c = EOF;
  
  if (fs_mutex_take ())
  {
    if (fp_valid (fp))
    {
      if (fp->mode_flag == 'R')
      {
        if (fp->position < fp->file_info.file_size)
        {
          if (fp->current_offset == page_data_size (fp->current_page))
          {
            if (!fs_advance_file (fp))
              return c;
          }
          else
          if (fp->data_buf_index == DATA_BUF_SIZE) // if big enough, this should never happen
          {
            fp->data_buf_index = 0;
            fp->data_buf_offset += DATA_BUF_SIZE;
            read_page_data (fp->current_page, fp->data_buf_offset, fp->data_buf, DATA_BUF_SIZE);
          }
          
          c = (int)fp->data_buf [fp->data_buf_index++];
          fp->current_offset ++;
          fp->position ++;
        }
        else
        {
          pseffs_error_callback (PSEFFS_ERROR_EOF);
        }
      }
      else
      {
        pseffs_error_callback (PSEFFS_ERROR_NO_PERMISSION);
      }
    }
    else
    {
      pseffs_error_callback (PSEFFS_ERROR_HANDLE);
    }
    
    fs_mutex_give();
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }
  
  return c;
}

//-------------------------------------------------------------------------------------
char *fgets( char *buf, int max_chars, FILE *fp )
{
  int count = 0;
    
  for ( ; count < max_chars ; )
  {
    int c = fgetc (fp);
    
    if (c >= 0)
    {
      buf [count++] = c;
    }
    else
    {
      break;
    }
      
    if (c == '\n')
    {
      break;
    }
  }
  
  if (count < max_chars)
    buf [count] = '\0';
  
  if (count) return buf;
  
  return NULL;
}

//-------------------------------------------------------------------------------------
size_t fread (void *ptr, size_t size, size_t nmemb, FILE *fp)
{
  size_t count = 0;
  
  uint8_t *buf = (uint8_t *)ptr;
    
  for (; count < nmemb; count++)
  {
    for (int j = 0; j < size; j++)
    {
      int c = fgetc (fp);
      
      if (c >= 0)
      {
        *buf++ = c;
      }
      else
      {
        return count;
      }
    }
  }
  
  return count;
}

////-------------------------------------------------------------------------------------
//// This function will get a specified number of characters from the file and place
//// them in the buffer, if it can't, it will fill up the remaining buffer spaces with
//// zeros
//void fs_fgetsposn (char *buf, int num_chars, FILE *fp, int start_pos)
//{
//  int count = 0;
//  int c;
//  
//  if (fp_valid (fp))
//  {
//    //debug_printf ("fgetsposn %u characters from file %s at position %u:\n\r", num_chars, file->file_info.name, start_pos));
//    if (fp->mode_flag == 'R')
//    {
//      fs_fsetpos (fp, start_pos);
//      while (count < num_chars && (c = fs_fgetc (fp)) >= 0)
//      {
//        buf [count++] = c;
//  //       if (fs_isprint (c))  {debug_printf (("%c", c));}
//  //       else              {debug_printf (("(0x%02bX)", c));}
//      }

//      while (count < num_chars)
//      {
//        buf [count++] = 0;
//      }
//    }
//   // debug_printf (("\n\r");
//  }
//  else
//  {
//    pseffs_error_callback (PSEFFS_ERROR_HANDLE);
//  }
//}

//-------------------------------------------------------------------------------------
static uint32_t next_link_num (uint32_t link_num)
{
  if (link_num == 0xFFFE) return 0;
  else return link_num + 1;
}

//-------------------------------------------------------------------------------------
static uint32_t previous_link_num (uint32_t link_num)
{
  if (link_num == 0) return 0xFFFE;
  else return link_num - 1;
}

//-------------------------------------------------------------------------------------
int stdout_putchar (int c);

//-------------------------------------------------------------------------------------
int fputc (int c, FILE *fp)
{
  if (fp == &__stdout || fp == &__stderr)
    return stdout_putchar (c);

  if (fs_mutex_take ())
  {
    if (fp_valid (fp)) // check permission
    {
      if (fp->mode_flag == 'W' || fp->mode_flag == 'A')   //random access
      {
        if (fp->data_buf_index == DATA_BUF_SIZE)
        {
          write_page_data (fp->current_page, fp->data_buf_offset, fp->data_buf, DATA_BUF_SIZE);
          memset (fp->data_buf, 0xFF, DATA_BUF_SIZE);
          fp->data_buf_offset += DATA_BUF_SIZE;
          fp->data_buf_index = 0;
        }
        
        if (fp->current_offset == page_data_size (fp->current_page))
        {
          if (fp->data_buf_index)
          {
            write_page_data (fp->current_page, fp->data_buf_offset, fp->data_buf, fp->data_buf_index);
            memset (fp->data_buf, 0xFF, DATA_BUF_SIZE);
            fp->data_buf_index = 0;
          }
          
          fp->data_buf_offset = 0;
          fp->current_offset = 0;
          
          fp->page_info.next_page = fs_find_open_page (); // get open page 

          if (fp->page_info.next_page)                            // got an open page
          {    // last page is a problem!!!!
            write_page_info (fp->current_page, (void *)&fp->page_info); // rewrite old with new next page

            fp->page_info.prev_page = fp->current_page;           // set new prev page
            fp->current_page = fp->page_info.next_page;
            fp->page_info.next_page = 0xFFFFFFFF;
            fp->page_info.link_num = next_link_num(fp->page_info.link_num);

            write_page_info (fp->current_page, (void *)&fp->page_info); // rewrite old with new next page
            
            if (fp->file_info.start_page == 0)                    // this is the first page of the file   ??????????
            {                                                     // dont have to worry about rebuild here
              fp->file_info.start_page = fp->page_info.prev_page;
            }
          }
          else  // file sys full - error critical at this stage
          {
            pseffs_error_callback (PSEFFS_ERROR_MEDIA_FULL);
            return EOF;
          }
        }
        
        fp->data_buf [fp->data_buf_index++] = c;
        fp->current_offset ++;
        fp->position ++;
        fp->file_info.file_size ++;
      }
      else
      {
        pseffs_error_callback (PSEFFS_ERROR_NO_PERMISSION);
        return EOF;
      }
    }
    else
    {
      pseffs_error_callback (PSEFFS_ERROR_HANDLE);
      return EOF;
    }
    
    fs_mutex_give ();
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
    return EOF;
  }
  
  return c;
}

//-------------------------------------------------------------------------------------
int fputs(const char *buf, FILE *fp)
{
  int count = 0;

 // debug_printf ("fputs %s in file %s\n\r", buf, file->file_info.name));
  while (buf [count])
  {
    if (fputc (buf [count], fp) != EOF)
    {
      count++;
    }
    else
    {
      return (EOF);
    }
  }
  
  return (count);
}

//-------------------------------------------------------------------------------------
size_t fwrite (const void *ptr, size_t size, size_t nmemb, FILE *fp)
{
  size_t count = 0;
  
  uint8_t *buf = (uint8_t *)ptr;
  
  for (; count < nmemb; count++)
  {
    for (int j = 0; j < size; j++)
    {
      if (fputc (*buf++, fp) == EOF)
      {
        return (count);
      }
    }
  }
  
  return (count);
}

//-------------------------------------------------------------------------------------
int fclose (FILE *fp)
{
  int ret = -1;
  
  if (fs_mutex_take ())
  {
   // debug_printf ("fclose %s mode %c\n\r", fp->file_info.name, fp->mode_flag));
    if (fp_valid (fp))
    {
      if (fp->mode_flag == 'W' || fp->mode_flag == 'A')
      {
        page_info_t page_info;
        
        read_page_info (fp->current_page, &page_info);
        
        if (memcmp (&page_info, &fp->page_info, sizeof (page_info_t)) != 0)
          write_page_info (fp->current_page, &fp->page_info);
        
        if (fp->data_buf_index)
          write_page_data (fp->current_page, fp->data_buf_offset, fp->data_buf, fp->data_buf_index);

        fs_fat_remove_num (fp->file_info.file_num);
        fs_fat_add (&fp->file_info);
      }

      fs_free_file_mem    (fp);
      
      ret = 0;
    }
    else
    {
      pseffs_error_callback (PSEFFS_ERROR_HANDLE);
    }
    
    fs_mutex_give();
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }

  return (ret);
}

//-------------------------------------------------------------------------------------
void file_list_cleanup (void *userdata)
{
//  FILE *fp = userdata;
}

//-------------------------------------------------------------------------------------
void fs_fcloseall (fs_t *fs)
{
  if (fs)
  {
    list_t *list_walker = fs->open_file_list;
  
    while ( list_walker )
    {
      FILE *fp = list_walk(&list_walker);
    
      fclose (fp);
    
      list_remove (&fs->open_file_list, fp, file_list_cleanup, NULL);

      fs->open_file_list_size--;
      
      if (fs->open_file_list_size)
      {
        debug_printf("file_list_cleanup %d\n\r", fs->open_file_list_size);
      }
      
      list_walker = open_file_list;
    }
  }
}

//-------------------------------------------------------------------------------------
int remove (const char *filename)
{
  int ret = 0;

  if (fs_mutex_take ())
  {
    FILE *fp;
    char uppercase_fname [MAX_FNAME_SIZE];

    //debug_printf ("remove file %s\n\r", filename));

    fp = fs_get_file_mem ();

    if (fp)
    {
      if (fs_format_fname (uppercase_fname, filename))
      {
        if (!fs_file_is_open (uppercase_fname) )
        {
          if (fs_find_file_info (uppercase_fname,fp))
          {
            if (fs_fat_remove_num (fp->file_info.file_num))
            {
              fs_remove_file_chain (fp->file_info.start_page);
            }
            else
            {
              ret = -1;
              pseffs_error_callback (PSEFFS_ERROR_MEDIA_ERROR);
            }
          }
          else
          {
            ret = -1;
            pseffs_error_callback (PSEFFS_ERROR_FILE_NOT_FOUND);
          }
          
          fs_free_file_mem (fp);
        }
        else
        {
          ret = -1;
          pseffs_error_callback (PSEFFS_ERROR_FILE_OPEN);
        }
      }
      else
      {
        ret = -1;
        pseffs_error_callback (PSEFFS_ERROR_ILEGALE_CHAR);
      }
    }
    else
    {
      ret = -1;
      pseffs_error_callback (PSEFFS_ERROR_OUT_OF_MEM);
    }
    
    fs_mutex_give ();
  }
  else
  {
    ret = -1;
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }

  return (ret);
}

/*-----------------------------------------------------------------------------------*/
bool fs_remove_num (uint32_t filenum)
{
  bool good = true;

  if (fs_mutex_take ())
  {
    FILE *fp;

    //debug_printf ("remove file num %u\n\r", filenum));

    fp = fs_get_file_mem ();

    if (fp)
    {
      if (fs_find_file_num (filenum,fp))
      {
        if (fs_fat_remove_num (filenum))
        {
          fs_remove_file_chain (fp->file_info.start_page);
        }
        else
        {
          good = false;
          pseffs_error_callback (PSEFFS_ERROR_MEDIA_ERROR);
        }
      }
      else
      {
        good = false;
        pseffs_error_callback (PSEFFS_ERROR_FILE_NOT_FOUND);
      }

      fs_free_file_mem (fp);
    }
    else
    {
      good = false;
      pseffs_error_callback (PSEFFS_ERROR_OUT_OF_MEM);
    }
    
    fs_mutex_give ();
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }

  return (good);
}

//-------------------------------------------------------------------------------------
int rename (const char *oldfile, const char *newfile)
{  
  int ret = 0;

  if (fs_mutex_take ())
  {
    FILE *fp;
    char uppercase_oldfile[MAX_FNAME_SIZE];
    char uppercase_newfile[MAX_FNAME_SIZE];

    //debug_printf ("rename file %s to %s\n\r", oldfile, newfile));

    fp = fs_get_file_mem();

    if ( fp )
    {
      if (fs_format_fname (uppercase_newfile, newfile))
      {
        if (!fs_file_is_open(uppercase_newfile))
        {
          if (fs_find_file_info(uppercase_newfile,fp))
          { // filename exists already
            ret = -1;
            pseffs_error_callback (PSEFFS_ERROR_FILE_NAME_IN_USE);
          }

          fp->file_info.file_num = 0; // clear mem
          fp->file_info.name[0] = 0; // clear mem
        }
        else
        {
          ret = -1;
          pseffs_error_callback (PSEFFS_ERROR_FILE_OPEN);
        }
      }
      else // file name ileagle or open
      {
        ret = -1;
        pseffs_error_callback (PSEFFS_ERROR_ILEGALE_CHAR);
      }

      if (ret == 0)
      {
        if (fs_format_fname (uppercase_oldfile, oldfile))
        {
          if (!fs_file_is_open (uppercase_oldfile))
          {
            if (fs_find_file_info (uppercase_oldfile,fp))
            {
              fs_fat_remove_num (fp->file_info.file_num);
            
              strcpy (fp->file_info.name, uppercase_newfile);
            
              fs_fat_add (&fp->file_info);
            }
            else
            {
              ret = -1;
              pseffs_error_callback (PSEFFS_ERROR_FILE_NOT_FOUND);
            }
          }
          else
          {
            ret = -1;
            pseffs_error_callback (PSEFFS_ERROR_FILE_OPEN);
          }
        }
        else // file name ileagle
        {
          ret = -1;
          pseffs_error_callback (PSEFFS_ERROR_ILEGALE_CHAR);
        }
      }

      fs_free_file_mem (fp);
    }
    else
    {
      ret = -1;
      pseffs_error_callback (PSEFFS_ERROR_OUT_OF_MEM);
    }

    fs_mutex_give();
  }
  else
  {
    ret = -1;
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }

  return (ret);
}

//-------------------------------------------------------------------------------------
int32_t fs_access (const char *fname)
{
  int32_t ret = -1;
  
  if (fs_mutex_take ())
  {
    char uppercase_fname [MAX_FNAME_SIZE];

    //debug_printf ("access %s\n\r", fname));
    if (fs_format_fname (uppercase_fname, fname))
    {
      file_info_t file_info;

      fs_reset_fat ();

      while (fs_fat_get_next_entry (&current_fat, &file_info))
      {
        if (file_info.file_num)
        {
          if (strcmp (file_info.name, uppercase_fname) == 0)  // check if filename is the one we are looking for
          {
            //debug_printf_file_info (&file_info);
            ret = ((int32_t) file_info.file_size);
            break;
          }
        }
      }

      if (ret < 0)
      {
        //debug_printf ("not found\n\r");
        pseffs_error_callback (PSEFFS_ERROR_FILE_NOT_FOUND);
      }
    }
    else
    {
      pseffs_error_callback (PSEFFS_ERROR_ILEGALE_CHAR);
    }
    
    fs_mutex_give();
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }

  return (ret);
}

//-------------------------------------------------------------------------------------
bool fs_fclip (const char *filename, uint32_t num_bytes)
{
  bool good = true;
  
  if (fs_mutex_take ())
  {
    if (num_bytes)
    {
      FILE *fp;
      char uppercase_fname [MAX_FNAME_SIZE];

      debug_printf ("fclip %u bytes from file %s\n\r", num_bytes, filename);

      fp = fs_get_file_mem ();

      if (fp)
      {
        if (fs_format_fname (uppercase_fname, filename))
        {
          if (!fs_file_is_open (uppercase_fname))
          {
            if (fs_find_file_info (uppercase_fname, fp))
            {
              if (num_bytes >= fp->file_info.file_size)
              {
                fs_free_file_mem (fp);
                fs_mutex_give ();
                return (remove (uppercase_fname));
              }

              fs_load_file (fp);

              debug_printf ("start_page %u, start_offset %u, file_size %u\n\r", fp->file_info.start_page, fp->file_info.start_offset, fp->file_info.file_size);

              while ((fp->current_offset + num_bytes) >= page_data_size (fp->current_page))
              {
                int page_size = page_data_size (fp->current_page);
                
                if (fs_advance_info (fp))
                {
                  num_bytes -= (page_size - fp->current_offset);
                  fp->file_info.file_size -= (page_size - fp->current_offset);
                  fp->file_info.start_page = fp->current_page;
                  fs_free_page (fp->page_info.prev_page);
                  
                  fp->current_offset = 0;
                }
                else
                { // file is as corrupt as an african politician
                  fs_free_file_mem (fp);
                  fs_mutex_give ();
                  return (remove (uppercase_fname));
                }
              }

              fp->current_offset += (uint32_t)num_bytes;
       
              fp->file_info.start_offset = fp->current_offset;
              fp->file_info.file_size -= num_bytes;
              
              
              if (fp->page_info.prev_page)
              { // update info in first file page
                fp->page_info.prev_page = 0;
                write_page_info (fp->current_page, &fp->page_info);
              }

              debug_printf ("start_page %u, start_offset %u, file_size %u\n\r", fp->file_info.start_page, fp->file_info.start_offset, fp->file_info.file_size);

              fs_fat_remove_num (fp->file_info.file_num);
              fs_fat_add (&fp->file_info);
            }
            else
            {
              good = false;
              pseffs_error_callback (PSEFFS_ERROR_FILE_NOT_FOUND);
            }
          }
          else
          {
            good = false;
            pseffs_error_callback (PSEFFS_ERROR_FILE_OPEN);
          }
        }
        else
        {
          good = false;
          pseffs_error_callback (PSEFFS_ERROR_ILEGALE_CHAR);
        }

        fs_free_file_mem(fp);
      }
      else
      {
        good = false;
        pseffs_error_callback (PSEFFS_ERROR_OUT_OF_MEM);
      }
    }
    
    fs_mutex_give();
  }
  else
  {
    good = false;
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }
  
  return (good);
}

//-------------------------------------------------------------------------------------
//void fsetpos (FILE *fp, uint32 position)
//{
//  uint32_t loaded_page;
//  uint32 difference;
//
//  debug_printf ("fsetpos in %s to %u\n\r", fp->file_info.name, position));
//  loaded_page = fp->current_page;
//  
//  if (position > fp->file_info.file_size)
//  { // beyond file end - just set to end of file
//    position = fp->file_info.file_size;
//  }
//  
//  if (position == fp->position)
//  { // Already in the right position
//    return;
//  }
//  
//  if (position > fp->position)
//  { // Need to step forward
//    difference = position - fp->position;
//    // Find right page
//    while (difference >= page_data_size)
//    {
//      difference -= page_data_size;
//      fp->position += page_data_size;
//      fs_advance_info (fp);
//    }
//    if ((difference + fp->current_offset) >= page_data_size)
//    {
//      difference -= (page_data_size - fp->current_offset);
//      fp->position += (page_data_size - fp->current_offset);
//      fp->current_offset = 0;
//      fs_advance_info (fp);
//    }
//  
//    fp->current_offset += difference;
//    fp->position += difference;
//  }
//  else
//  { // Need to step back
//    difference = fp->position - position;
//  
//    // Find right page
//    while (difference >= page_data_size)
//    {
//      difference -= page_data_size;
//      fp->position -= page_data_size;
//      fs_reverse_info (fp);
//    }
//    if (difference > fp->current_offset)
//    {
//      difference -= fp->current_offset;
//      fp->position -= fp->current_offset;
//      //Can set offset to data size here, 'cause it'll get decremented below
//      fp->current_offset = page_data_size;      
//      fs_reverse_info (fp);
//    }
//  
//    fp->current_offset -= difference;
//    fp->position -= difference;
//  }
//  
//  if (loaded_page != fp->current_page)
//  {
//    read_page_data(fp->current_page, fp->buffer);
//  }
//}


//-------------------------------------------------------------------------------------
static void setpos (FILE *fp, uint32_t position)
{
 // debug_printf ("fsetpos in %s to %u\n\r", fp->file_info.name, position));

  if (fs_mutex_take ())
  {
    if (fp->mode_flag == 'R') /*enable seek on read and write*/
    {
      if (position > fp->file_info.file_size)
      { // beyond file end - just set to end of file
        position = fp->file_info.file_size;
      }
      
      if (position == fp->position)
      { // Already in the right position
        fs_mutex_give();
        return;
      }
    
      if (position > fp->position)
      { // Need to step forward
        while (fp->position < position)
        {
          int diff = position - fp->position;
          
          if ((fp->current_offset + diff) > page_data_size (fp->current_page))
          {
            int page_size = page_data_size (fp->current_page);
            
            if (fs_advance_info (fp))
            {
              fp->position += (page_size - fp->current_offset);
              fp->current_offset = 0;
            }
            else
              break; // from while
          }
          else
          {
            fp->current_offset += diff;
            fp->position += diff;
          }
        }
      }
      else
      {
        while (fp->position > position)
        {
          int diff = fp->position - position;
          
          if (diff > fp->current_offset)
          {
            if (fs_reverse_info (fp))
            {
              fp->position -= (fp->current_offset + 1);
              fp->current_offset = page_data_size (fp->current_page) - 1;
            }
            else
              break; // from while
          }
          else
          {
            fp->current_offset -= diff;
            fp->position -= diff;
          }
        }
      }
    
      read_page_data (fp->current_page, fp->current_offset, fp->data_buf, DATA_BUF_SIZE);
      fp->data_buf_offset = fp->current_offset;
      fp->data_buf_index = 0;
    }
    else
    {
      pseffs_error_callback (PSEFFS_ERROR_NO_PERMISSION);
    }
    
    fs_mutex_give ();
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }
}

//-------------------------------------------------------------------------------------
//uint32 fseekend (FILE *fp)
//{
//  uint32 file_size = 0;
//
//  debug_printf ("fseekend of file %s\n\r", fp->file_info.name));
//  fsetpos (fp, fp->file_info.file_size);
//
//  return (fp->position);
//}

//-------------------------------------------------------------------------------------
static uint32_t seekend (FILE *fp)
{
 // debug_printf ("fseekend of file %s\n\r", fp->file_info.name));

  if (fs_mutex_take ())
  {
    while (fp->position < fp->file_info.file_size)
    {
      int diff = fp->file_info.file_size - fp->position;
      
      if ((fp->current_offset + diff) > page_data_size (fp->current_page))
      {
        int page_size = page_data_size (fp->current_page);
        
        if (fs_advance_info (fp))
        {
          fp->position += page_size - fp->current_offset;
          fp->current_offset = 0;
        }
        else
          break; // from while
      }
      else
      {
        fp->current_offset += diff;
        fp->position += diff;
      }
    }
        
    read_page_data (fp->current_page, fp->current_offset, fp->data_buf, DATA_BUF_SIZE);
    fp->data_buf_offset = fp->current_offset;
    fp->data_buf_index = 0;
    fs_mutex_give ();
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_OS_LOCK);
  }

  return (fp->position);
}

//-------------------------------------------------------------------------------------
void fs_freverse (FILE *fp, uint32_t numchars)
{
  //debug_printf ("freverse %u characters in file %s\n\r", numchars, fp->file_info.name));
  setpos (fp, fp->position - numchars);
}

//-------------------------------------------------------------------------------------
#ifdef FILE_PRINTF
static int file_putchar (int c)
{
  if (putchar_f)
  {
    fputc (c, putchar_f);
  }

  return (c);
}
#endif


//-------------------------------------------------------------------------------------
int ferror (FILE *f) {
  /* Your implementation of ferror */
  return EOF;
}

//-------------------------------------------------------------------------------------
long int ftell (FILE *stream)
{
  return stream->position;
}

//-------------------------------------------------------------------------------------
int fseek (FILE *f, long int offset, int nMode)
{
  switch (nMode)
  {
    case SEEK_SET: /* start of stream (see fseek) */
      if (offset > f->file_info.file_size)
        return EOF;
      
      setpos (f, offset);
      break;
    case SEEK_CUR: /* current position in stream (see fseek) */
      if ((f->position + offset) > f->file_info.file_size)
        return EOF;
      
      setpos (f, f->position + offset);
      break;
    case SEEK_END: /* end of stream (see fseek) */
      if (offset)
        return EOF;
      
      seekend (f);
      break;
  }
  
  return 0;
}


//-------------------------------------------------------------------------------------
#ifdef FILE_PRINTF
void file_printf (FILE *f, const char *ptr, ...)
{
  if (fs_mutex_take ())
  {
    extern int (*putchar_func_pointer)(int);
    int (*putchar_ptr_save)(int) = putchar_func_pointer;
    __va_list var_ptr;
    va_start (var_ptr, ptr);
    putchar_func_pointer = file_putchar;
    putchar_f = f;
    (void)vprintf (ptr, var_ptr);
    va_end (var_ptr);
    putchar_func_pointer = putchar_ptr_save;
    putchar_f = 0;
  
    fs_mutex_give ();
  }
}
#endif

//-------------------------------------------------------------------------------------
// File Systems IO Functions
//-------------------------------------------------------------------------------------
//static void fs_kill_page (uint32_t page)
//{
//  //debug_printf ("fs_kill_page %u\n\r", page));
//  Erase_Page(page);
//}

//-------------------------------------------------------------------------------------
static void fs_free_page (uint32_t page)
{
  page_info_t info = {0,0,0,0};
  debug_printf ("fs_free_page %u\n\r", page);
  write_page_info (page, (void *)&info);
}

////-------------------------------------------------------------------------------------
//static void fs_free_page (uint32_t page)
//{
//  page_info_t info;
//  
//  debug_printf ("fs_free_page %u\n\r", page));
//  read_page_info   (page, &info);     // read info from flash (8 bytes)
//  info.file_num = 0;                          // clear file number (make page available)
////  Write_Buffer (FS_PAGE_IDX, &info, FS_PAGE_SIZE);  // update page info in buffer
//  write_page_info(page, &info);            // over write file num to zero
//}

//-------------------------------------------------------------------------------------
static uint32_t fs_check_file_chain (uint32_t file_page)
{
  page_info_t         page_info;
  uint32_t            file_num,
                      link_num;
  uint32_t            start_page,
                      curr_page,
                      last_page;

  //debug_printf ("check_file_chain from %u\n\r", file_page));

  if (file_page < file_system_start_page || file_page > file_system_end_page)
  {
    debug_printf ("invalid page\n\r");
    return (0);
  }

  read_page_info (file_page, &page_info);

  if (page_info.file_num == 0 || page_info.file_num == 0xFFFFFFFF)
  { // this page is not a valid file page
    debug_printf ("page is not a file page\n\r");
    return (0);
  }
  
  if (page_info.prev_page > file_system_end_page)
  { // this page is not a valid file page
    debug_printf ("prev page is not a valid page %hX\n\r", page_info.prev_page);
    return (0);
  }
  
  if (page_info.next_page > file_system_end_page && page_info.next_page != 0xFFFFFFFF)
  { // this page is not a valid file page
    debug_printf ("next page is not a valid page %hX\n\r", page_info.next_page);
    return (0);
  }

  file_num = page_info.file_num;
  link_num = page_info.link_num;
  curr_page = file_page;
  last_page = curr_page;
  start_page = curr_page;

  //debug_printf(("checking integrity for file %u\n\r",file_num)); 

  while (page_info.prev_page)   // backward
  {
    curr_page = page_info.prev_page;
   // debug_printf (("reading page %u\n\r",curr_page));
    read_page_info (curr_page, &page_info);

    if (file_num != page_info.file_num)
    {
      debug_printf ("failed with incorrect file_num %u\n\r", page_info.file_num);
      return (0);
    }
    if (link_num != next_link_num (page_info.link_num))
    {
      debug_printf ("failed with incorrect link_num %u, should be %u\n\r", page_info.link_num,(page_info.link_num+1));
      return (0);
    }
    if (page_info.next_page != last_page)
    {
      debug_printf ("failed with incorrect next_page %u, should be %u\n\r", page_info.next_page, last_page);
      return (0);
    }

    link_num = previous_link_num (link_num);
    last_page = curr_page;
    start_page = curr_page;
  }

  if (curr_page != file_page)
  {
    read_page_info (file_page, &page_info);
  }

  link_num = page_info.link_num;
  curr_page = file_page;
  last_page = curr_page;

  while (page_info.next_page && page_info.next_page != 0xFFFFFFFF)   // forward
  {
    curr_page = page_info.next_page;
    //debug_printf (("reading page %u\n\r",curr_page));
    read_page_info (curr_page, &page_info);

    if (file_num != page_info.file_num )
    {
      debug_printf ("failed with incorrect file_num %u\n\r", page_info.file_num);
      return (0);
    }
    if (link_num != previous_link_num (page_info.link_num))
    {
      debug_printf ("failed with incorrect link_num %u, should be %u\n\r", page_info.link_num, (page_info.link_num - 1) );
      return (0);
    }
    if (page_info.prev_page != last_page)
    {
      debug_printf ("failed with incorrect prev_page %u, should be %u\n\r", page_info.prev_page, last_page);
      return (0);
    }

    link_num = next_link_num (link_num);
    last_page = curr_page;
  }

  return (start_page);
}

//-------------------------------------------------------------------------------------
static void fs_remove_file_fragments (uint32_t file_num)
{
  page_info_t page_info;
  uint32_t page;                       // page index - hopefully the memory device has less than 64k pages else mak this 32 bit

  debug_printf ("fs_remove_file_fragments for file %hX\n\r", file_num);
  for (page = file_system_start_page ; page <= file_system_end_page ; page++)
  {
    read_page_info (page, &page_info);
    if (page_info.file_num == file_num)   // found a file page
    {
      debug_printf ("found file %hX fragment @ page %u\n\r", file_num, page);
      fs_free_page (page);
    }
    
    fragment_search_callback (file_num, page);
  }
}

//-------------------------------------------------------------------------------------
static void fs_remove_file_chain (uint32_t page)
{
  page_info_t page_info;
  uint32_t file_num, curr_page = page;

  //debug_printf ("fs_remove_file_chain from %u\n\r", page));

  if (page < file_system_start_page || page > file_system_end_page)
  {
    debug_printf ("fs_remove_file_chain - invalid page %u\n\r", page);
    return;
  }

  read_page_info (curr_page, &page_info);

  if (page_info.file_num == 0 || page_info.file_num == 0xFFFFFFFF)
  { // this page is free
    debug_printf ("page was free\n\r");
    return;
  }

  file_num = page_info.file_num;

  while (page_info.prev_page                            &&
        (page_info.prev_page >= file_system_start_page) &&
        (page_info.prev_page <= file_system_end_page)) // backward
  {
    curr_page           = page_info.prev_page;
    read_page_info      (curr_page, &page_info);
    
    if (file_num == page_info.file_num)
    {
      debug_printf ("fs_remove_file_chain backwards %u\n\r", curr_page);
      fs_free_page (curr_page);
    }
    else
    {
      fs_remove_file_fragments (file_num);
      return;
    }
  }

  if (curr_page != page)
  {
    curr_page = page;
    read_page_info (curr_page, &page_info);
  }

  while (page_info.next_page                            &&
         page_info.next_page != 0xFFFFFFFF              &&
        (page_info.next_page >= file_system_start_page) &&
        (page_info.next_page <= file_system_end_page)) // forward
  {
    curr_page = page_info.next_page;
    read_page_info (curr_page, &page_info);
    if (file_num == page_info.file_num)
    {
      debug_printf ("fs_remove_file_chain forward %u\n\r", curr_page);
      fs_free_page (curr_page);
    }
    else
    {
      fs_remove_file_fragments (file_num);
      return;
    }
  }

  debug_printf ("fs_remove_file_chain current %u\n\r", page);
  fs_free_page (page);
}

//-------------------------------------------------------------------------------------
static bool  fs_advance_file (FILE * fp)
{
  uint32_t next_link, next_page;

 // debug_printf ("advance_file %s\n\r", fp->file_info.name));
  if (fp->page_info.next_page && fp->page_info.next_page != 0xFFFFFFFF)
  {
    next_link = next_link_num (fp->page_info.link_num);
    next_page = fp->page_info.next_page;

    read_page_info (next_page, &fp->page_info);

    if ((fp->page_info.file_num == fp->file_info.file_num)
      && (fp->page_info.link_num == next_link))
    {
      fp->current_page = next_page;       // set to next flash page
      fp->current_offset = 0;             // reset file index
  
      fp->data_buf_offset = 0;
      fp->data_buf_index = 0;
      read_page_data (fp->current_page, fp->data_buf_offset, fp->data_buf, DATA_BUF_SIZE);
      return (true);
    }

    read_page_info (fp->current_page, &fp->page_info); // reload current info
  }
  
  pseffs_error_callback (PSEFFS_ERROR_NO_NEXT_PAGE);
  
  return (false);
}

//-------------------------------------------------------------------------------------
static bool page_not_in_ram (uint32_t page)
{
  list_t *list_item = open_file_list;

 // debug_printf ("page_not_in_ram %u\n\r", page));

  while (list_item)
  { //First check open files
    if (((FILE *)&list_item->memory)->current_page == page)
    {
      return (false);
    }
    
    list_item = list_item->next;
  }
  
  if (current_fat.current_page == page)
    return (false);

  return (true);
}

//-------------------------------------------------------------------------------------
static uint32_t fs_find_open_page(void)
{
  bool          over_wear = false;
  uint32_t      end_search_page;
  sector_info_t sector_info;
  page_info_t   pagei;
  uint32_t      free_pages_per_sector = 0xFFFFFFFF;

//  debug_printf ("fs_find_open_page\n\r");
  
  if (start_search_page < file_system_start_page
    || start_search_page > file_system_end_page)
    start_search_page = file_system_start_page;
  
  end_search_page = start_search_page++;

 // debug_printf(("fs_find_open_page start @ %u\n\r",start_search_page));

  sector_info.bytes.flags = 0; // force sector info read

  do
  {
    for ( ; start_search_page <= file_system_end_page; start_search_page++)
    {
      if ((start_search_page % flash_pages_per_sector == 0)
        || (sector_info.bytes.flags != ERASE_COUNT_KEY))
        read_page_sector_head (start_search_page, &sector_info);

      if (sector_info.bytes.flags != ERASE_COUNT_KEY)
      {
        debug_printf ("Sector check failed @ page %d (sector %d)\n\r", start_search_page, start_search_page / flash_pages_per_sector);
        flash_driver_erase_sector_address (page_sector_address (start_search_page), false);   // wipe sector 70 ms
        sector_info.erase_count = 0;                                  // set_erase_count first
        sector_info.bytes.flags = ERASE_COUNT_KEY;
        write_page_sector_head (start_search_page, &sector_info); // rewrite erase count
        return (start_search_page);
      }
      
      if ((sector_info.erase_count & ERASE_COUNT_MASK) < wear_threshold)
      {
        read_page_info (start_search_page, &pagei);

        if (pagei.file_num == 0xFFFFFFFF) // cleared and ready to be used
        {
          if (page_not_in_ram (start_search_page)) // not in use by open file
          {
            // debug_printf(("using - %u\n\r",start_search_page));
            return (start_search_page);
          }
          
          free_pages_per_sector = 0xFFFFFFFF; // cant erase this sector
        }
        else
        if (pagei.file_num == 0) // ready to be erased - check if whole sector can be erased
        {
          if (start_search_page % flash_pages_per_sector == 0) // first page per sector
          {
            free_pages_per_sector = 1;
          }
          else
          {
            if (free_pages_per_sector < flash_pages_per_sector)
              free_pages_per_sector++;
          }
          
          if (free_pages_per_sector == flash_pages_per_sector)
          {
            debug_printf ("garbage collected @ sector %d!\n\r", start_search_page / flash_pages_per_sector);
            flash_driver_erase_sector_address (page_sector_address (start_search_page), false); // wipe sector
            sector_info.erase_count++;                                    // inc erase count
            sector_info.bytes.flags = ERASE_COUNT_KEY;
            write_page_sector_head (start_search_page, &sector_info); // rewrite erase count
            return (start_search_page); // this is the last page in this sector but so be it
          }
        }
        else // still in use
        {
          free_pages_per_sector = 0xFFFFFFFF; // cant erase this sector
        }
      }
      else
      {
        over_wear = true;
      }

//      else if ( pagei.file_num == 0xFFFFFFFF && !dead_page )
//      {
//        dead_page = start_search_page;
//      }
    }

    sector_info.bytes.flags = 0;
    free_pages_per_sector = 0xFFFFFFFF;
    
    for (start_search_page = file_system_start_page; start_search_page <= end_search_page ; start_search_page++)
    {
      if ((start_search_page % flash_pages_per_sector == 0)
        || (sector_info.bytes.flags != ERASE_COUNT_KEY))
        read_page_sector_head (start_search_page, &sector_info );
      
      if (sector_info.bytes.flags != ERASE_COUNT_KEY)
      {
        debug_printf ("Sector check failed @ page %d (sector %d)\n\r", start_search_page, start_search_page / flash_pages_per_sector);
        flash_driver_erase_sector_address (page_sector_address (start_search_page), false); // wipe sector
        sector_info.erase_count = 0;                                  // set_erase_count first
        sector_info.bytes.flags = ERASE_COUNT_KEY;
        write_page_sector_head (start_search_page, &sector_info); // rewrite erase count
        return (start_search_page);
      }
      
      if ((sector_info.erase_count & ERASE_COUNT_MASK) < wear_threshold)
      {
        read_page_info (start_search_page, &pagei);

        if (pagei.file_num == 0xFFFFFFFF) // cleared and ready to be used
        {
          if (page_not_in_ram (start_search_page)) // not in use by open file
          {
            // debug_printf(("using - %u\n\r",start_search_page));
            return (start_search_page);
          }
          
          free_pages_per_sector = 0xFFFFFFFF; // cant erase this sector
        }
        else
        if (pagei.file_num == 0) // ready to be erased - check if whole sector can be erased
        {
          if (start_search_page % flash_pages_per_sector == 0) // first page per sector
          {
            free_pages_per_sector = 1;
          }
          else
          {
            if (free_pages_per_sector < flash_pages_per_sector)
              free_pages_per_sector++;
          }

          if (free_pages_per_sector == flash_pages_per_sector)
          {
            debug_printf ("garbage collected @ sector %d!\n\r", start_search_page / flash_pages_per_sector);
            flash_driver_erase_sector_address (page_sector_address (start_search_page), false); // wipe sector
            sector_info.erase_count++;                                    // inc erase count
            sector_info.bytes.flags = ERASE_COUNT_KEY;
            write_page_sector_head (start_search_page, &sector_info); // rewrite erase count
            return (start_search_page); // this is the last page in this sector but so be it
          }
        }
        else // still in use
        {
          free_pages_per_sector = 0xFFFFFFFF; // cant erase this sector - set high to fail test
        }
      }
      else
      {
        over_wear = true;
      }
    }
    
    if (over_wear && (wear_threshold < MAX_WEAR_THRESHOLD))
    {
      wear_threshold += WEAR_THRESHOLD;
    }
//    else if ( dead_page ) // may as well try to revive a dead page - we dont have any other space
//    {
//      if ( dead_page_revive_count++ < 10 )
//      {
//        start_search_page = dead_page;
//        return (start_search_page);
//      }
//      else
//      {
//        return(0); // we have tried enough times to use a dead page - we may be caught in a loop trying to use dead pages that are realy damaged
//      }
//    }
    else
    {
      return (0); // disk full
    }
  } while( 1 );
}

//-------------------------------------------------------------------------------------
//find an available file block by checking the file number entry
static FILE *fs_get_file_mem (void)
{
  FILE *file = list_add (&open_file_list, sizeof(FILE), "file");
  
  if (file)
  {
    open_file_list_size++;
    return file;
  }
  
  pseffs_error_callback (PSEFFS_ERROR_OUT_OF_MEM);
  
  return NULL;
}

//-------------------------------------------------------------------------------------
static void fs_free_file_mem (FILE *fp)
{
  if (fp)
  {
    list_remove (&open_file_list, fp, file_list_cleanup, NULL);
  }
  else
  {
    debug_printf ("free_file_mem has no file pointer\n\r");
  }
}

//-------------------------------------------------------------------------------------
static bool fp_valid (FILE *fp)
{
  list_t *list_item = open_file_list;
  
  while (list_item)
  {
    if ((FILE *)&list_item->memory == fp)
      return true;
    
    list_item = list_item->next;
  }
  
  return false;
}

//-------------------------------------------------------------------------------------
static bool  fs_format_fname (char *fname_buf, const char *fname)
{
  int i;
  
  if (!fname_buf || !fname) return false;
  
 // debug_printf ("fs_format_fname %s ", fname));
  for (i = 0; i < MAX_FNAME_SIZE; i++)
  {
    if (fname[i] == '\0')
    {
      break;
    }

    if (fs_isprint ((int)fname[i]) && fname[i] > ',' && fname[i] <= '~')
    {
      fname_buf[i] = fs_toupper (fname[i]);
    }
    else
    {
//      fname_buf[i] = '\0';
      debug_printf ("Ileagle char found -> 0x%02hhX @ %d !!!\n\r", fname[i], i);
      fname_buf[i] = '\0';
      return(false);
    }
  }

  if (i < MAX_FNAME_SIZE) {fname_buf[i] = '\0';}
  else return(false);

 // debug_printf (("-> %s\n\r", fname_buf));

  return(true);
}

//-------------------------------------------------------------------------------------
// Must pass uppercase filename to this function
static bool fs_file_is_open (char *fname)
{
  list_t *list_walker = open_file_list;
  
  while (list_walker)
  {
    FILE *file = list_walk (&list_walker);
    
    if (strcmp (file->file_info.name, fname) == 0)
    {
      pseffs_error_callback (PSEFFS_ERROR_FILE_OPEN);
    //  debug_printf ("File %s is open\n\r", fname));
      return (true);
    }
  }

  //debug_printf ("File %s not open\n\r", fname));
  return (false);
}

//-------------------------------------------------------------------------------------
// Must pass uppercase filename to this function
static bool  fs_find_file_info (char *uppercase_fname, FILE *fp)
{
  //debug_printf ("fs_find_file_info for %s\n\r", uppercase_fname));
  
  if (!fp) return false;

  strcpy (fp->file_info.name, uppercase_fname); // ????? why do you do this Hein ?????
  
  fs_reset_fat ();

  while (fs_fat_get_next_entry (&current_fat, &fp->file_info))
  {
    if (fp->file_info.file_num)
      if (strcmp (uppercase_fname, fp->file_info.name) == 0)
        return (true);
  }

//  debug_printf ("fs_find_file_info Not found %s\n\r",uppercase_fname));
  return (false);
}

//-------------------------------------------------------------------------------------
// Must pass uppercase filename to this function
static bool fs_find_file_num (uint32_t filenum, FILE *fp)
{
 // debug_printf ("fs_find_file_num %u\n\r", filenum));

  fs_reset_fat ();

  while (fs_fat_get_next_entry (&current_fat, &fp->file_info))
  {
    if (fp->file_info.file_num == filenum)
    {
     // debug_printf_file_info (&fp->file_info);
      return (true);
    }
  }

 // debug_printf ("fs_find_file_num Not found %d\n\r",filenum));
  return (false);
}

//-------------------------------------------------------------------------------------
static void fs_load_file (FILE *fp)
{
 // debug_printf ("load_file %s\n\r", fp->file_info.name));
  fp->current_page = fp->file_info.start_page;  // reset file to start page & offset
  fp->current_offset = fp->file_info.start_offset;
  fp->position = 0;
  fp->data_buf_offset = fp->current_offset;
  fp->data_buf_index = 0;
  
  read_page_info (fp->current_page, &fp->page_info);
  read_page_data (fp->current_page, fp->data_buf_offset, fp->data_buf, DATA_BUF_SIZE);
}

//-------------------------------------------------------------------------------------
static bool fs_advance_info (FILE *fp)
{
  uint32_t next_link, next_page;

 // debug_printf ("fs_advance_info of file %s\n\r", fp->file_info.name));
  if (fp->page_info.next_page && fp->page_info.next_page != 0xFFFFFFFF)
  {
    next_link = next_link_num (fp->page_info.link_num);
    next_page = fp->page_info.next_page;

    read_page_info (next_page, &fp->page_info);

    if ((fp->page_info.file_num == fp->file_info.file_num)
      && (fp->page_info.link_num == next_link))
    {
      fp->current_page = next_page;       // set to next fat mem page
      return (true);
    }

    read_page_info (fp->current_page, &fp->page_info);
  }

  debug_printf ("fs_advance_info of file %s failed!\n\r", fp->file_info.name);
  return (false);
}

//-------------------------------------------------------------------------------------
static bool  fs_reverse_info (FILE *fp)
{
  uint32_t prev_link, prev_page;

  //debug_printf ("fs_reverse_info of file %s\n\r", fp->file_info.name));
  if (fp->page_info.prev_page)
  {
    prev_link = previous_link_num (fp->page_info.link_num);
    prev_page = fp->page_info.prev_page;

    read_page_info (prev_page, &fp->page_info);

    if ((fp->page_info.file_num == fp->file_info.file_num)
      && (fp->page_info.link_num == prev_link))
    {
      fp->current_page = prev_page;       // set to next fat mem page
      return (true);
    }

    read_page_info (fp->current_page, &fp->page_info);
  }

  debug_printf ("fs_reverse_info of file %s failed!\n\r", fp->file_info.name);
  return (false);
}

//-------------------------------------------------------------------------------------
static bool fs_get_file_num (FILE *fp)
{
  list_t *list_item = open_file_list;
  file_info_t spare_info;
  uint32_t first_file_num = next_file_num;

  //debug_printf ("fs_get_file_num\n\r");
  
  if (!fp) return false;

  do
  {
    fs_reset_fat();
  
    while (fs_fat_get_next_entry (&current_fat, &spare_info))
    {
      if (spare_info.file_num == next_file_num)
      {
        fs_reset_fat (); // restart search with new file num
        next_file_num++;

        if (next_file_num >= FIRST_FAT_FILE_NUM)
        {
          next_file_num = 1;
        }
        
        if (next_file_num == first_file_num) // searched all posible file numbers
          return false;
      }
    }

    while (list_item)
    {
      if (((FILE *)&list_item->memory)->file_info.file_num == next_file_num)
      {
        list_item = open_file_list; // restart search with new file num
        next_file_num ++;

        if (next_file_num >= FIRST_FAT_FILE_NUM)
        {
          next_file_num = 1;
        }
        
        if (next_file_num == first_file_num) // searched all posible file numbers
          return false;
        
        break; // from while ( list_item )
      }
      
      list_item = list_item->next;
    }
    
    if (!list_item) // nothing found in ram
    {
      fp->file_info.file_num  = next_file_num;
      return true;
    }
    
  } while (1);
}

//-------------------------------------------------------------------------------------
static bool  fs_init_new_file (char *uppercase_fname, FILE *fp)
{
 // debug_printf ("init_new_file %s\n\r", uppercase_fname));

  if (fp)
  {
    if (fs_get_file_num (fp))
    {
      strcpy (fp->file_info.name, uppercase_fname);
  
      fp->file_info.start_page = fs_find_open_page();
     // debug_printf ("start_page %u\n\r", fp->file_info.start_page));
      fp->file_info.start_offset = 0;
      fp->file_info.file_size = 0;
  
      if (!fp->file_info.start_page)
        return (false);
  
      fp->page_info.file_num = fp->file_info.file_num;
      fp->page_info.prev_page = 0;
      fp->page_info.next_page = 0xFFFFFFFF;
      fp->page_info.link_num = 0;
      
      write_page_info (fp->file_info.start_page, &fp->page_info);
    
      fp->current_page = fp->file_info.start_page;
      fp->current_offset = 0;
      fp->position = 0;

      fp->data_buf_offset = 0;
      fp->data_buf_index = 0;
      memset (fp->data_buf, 0xFF, DATA_BUF_SIZE);
      
      return (true);
    }
  }

  return (false);
}

//-------------------------------------------------------------------------------------
#ifdef RESET_FILE
static void fs_reset_file (FILE *fp)
{
 // debug_printf ("reset_file %s\n\r", fp->file_info.name));
  if (fp->current_page != fp->file_info.start_page)
  {
    fs_load_file (fp);
  }
  else
  {
    fp->current_offset = fp->file_info.start_offset;
    fp->position = 0;      
  }
}
#endif

//-------------------------------------------------------------------------------------
void reset_file_list(void)
{
  if (fs_mutex_take())
  {
    fs_reset_fat();
    fs_mutex_give();
  }
}

/*-----------------------------------------------------------------------------------*/
int32_t read_file_list (char *name, uint32_t *filenum, uint32_t *filesize)
{
  if (fs_mutex_take())
  {
    file_info_t spare_info;
   // debug_printf ("read_file_list "));
    
    while (fs_fat_get_next_entry (&current_fat, &spare_info))
    {
      if (spare_info.file_num)
      {
        if (spare_info.name [0])
        {
          strncpy (name, spare_info.name, MAX_FNAME_SIZE);
      //    debug_printf (("-> %s\n\r", name));
          if (filenum)  *filenum  = spare_info.file_num;
          if (filesize) *filesize = spare_info.file_size;
          fs_mutex_give ();
          return ((int32_t)spare_info.file_size);
        }
      }
    }
    
    debug_printf ("NOT_FOUND\n\r");
    
    fs_mutex_give();
  }
  
  return (-1);
}

//-------------------------------------------------------------------------------------
void Format_fs (void)
{
  page_info_t page_info;
  unsigned int page_number;

  if (!flash_driver_init ())
  { // big problem
    media_error_callback ("NO FLASH MEDIA AVAILABLE");
    return;
  }
  
  fs_fcloseall ();
  
  if (fs_mutex_take ())
  {
    sector_info_t sector_info = {0};

    //debug_printf ("Format_fs\n\r"));
    for (page_number = file_system_start_page ; page_number <= file_system_end_page ; page_number++)
    {
      if ((page_number % flash_pages_per_sector == 0)
        || (sector_info.bytes.flags != ERASE_COUNT_KEY))
        read_page_sector_head (page_number, &sector_info );
      
      if (sector_info.bytes.flags != ERASE_COUNT_KEY)
      {
        debug_printf ("Sector check failed @ page %d (sector %d)\n\r", page_number, page_number / flash_pages_per_sector);
        flash_driver_erase_sector_address (page_sector_address (page_number), false);               // wipe sector
        sector_info.erase_count = 0;                                    // set_erase_count first
        sector_info.bytes.flags = ERASE_COUNT_KEY;
        write_page_sector_head (page_number, &sector_info);  // rewrite erase count
      }

      Format_Progress_Callback (page_number, file_system_end_page);

      read_page_info (page_number, &page_info);

      if (page_info.file_num != 0) // && page_info.file_num != 0xFFFFFFFF ) we dont know for sure if the page contents are clean - rather kill it off for garbage collection
      {
        fs_free_page (page_number);
      }
    }
    
    flash_driver_save_quick_start_fat (0);
    memset (&current_fat, 0, sizeof (FAT));
    fs_mutex_give ();
  }
}

//-------------------------------------------------------------------------------------
// Small FAT Functions
//-------------------------------------------------------------------------------------
static bool fs_init_fat (FAT *fat)
{
  if (fat)
  {
    if (fs_find_fat (fat))
      return true;
      
    if (fs_create_fat (fat, FIRST_FAT_FILE_NUM))
      return true;
  }
  
  debug_printf ("fs_init_fat failed!\n\r");
  return (false);
}

//-------------------------------------------------------------------------------------
static void fs_reset_fat (void)
{
  //debug_printf ("fs_reset_fat - "));

  if (current_fat.start_page)
  {
    current_fat.current_offset = 0;        // fat must always start on the first char of the page

    if (current_fat.current_page != current_fat.start_page)
    {
      current_fat.current_page = current_fat.start_page;  // reset fat to start
      
      read_page_info (current_fat.current_page, &current_fat.page_info);

     // debug_printf ("reloaded info/buffer\n\r");
    }

   // debug_printf ("start page: %u\n\r", fat.start_page));
  }
  else
  {
    debug_printf ("fs_reset_fat - failed - no start page\n\r");
    fs_init_fat (&current_fat);
  }
}

//-------------------------------------------------------------------------------------
static bool fs_find_fat (FAT *fat)
{
  page_info_t   page_info;
  sector_info_t sector_info;
  uint32_t      saved_fat_page;
  uint32_t      better_fat_file_num;

  if (!fat)     return false;
    
  if (!flash_driver_init ())
  { // big problem
    media_error_callback ("NO FLASH MEDIA AVAILABLE");
    return false;
  }
  
  uint32_t quick_start_fat_page = flash_driver_load_quick_start_fat ();
  
  if (quick_start_fat_page
    && quick_start_fat_page <= file_system_end_page
    && quick_start_fat_page >= file_system_start_page)
  {
    read_page_sector_head (quick_start_fat_page, &sector_info);

    if (sector_info.bytes.flags == ERASE_COUNT_KEY) // not in an erased or formatted sector
    {
      read_page_info (quick_start_fat_page, &page_info);
      
      if (page_info.file_num >= FIRST_FAT_FILE_NUM
        && page_info.file_num != 0xFFFFFFFF)   // its a fat page - lets use it
      {
        debug_printf ("using fat @ %u\n\r", quick_start_fat_page);
        fat->start_page = fs_check_file_chain (quick_start_fat_page);

        if (fat->start_page)
        {
          fat->current_page = fat->start_page;
          fat->current_offset = 0;
          read_page_info (fat->start_page, &fat->page_info);
          debug_printf ("flash_driver_load_quick_start_fat @ page %u (sector %u)\n\r", fat->start_page, fat->start_page / flash_pages_per_sector);
          return (true);
        }
        else // fat file chain broken - wipe it look for a previous version.
        {    // we may have died while writing the fat so file fragments may remain - scan will clear it out
          debug_printf ("flash_driver_load_quick_start_fat failed\n\r");
          fs_remove_file_chain (quick_start_fat_page);
        }
      }
      else
      {
        debug_printf ("flash_driver_load_quick_start_fat @ page %u failed with page info (%u, %u, %u, %u)\n\r"
                                          , quick_start_fat_page
                                          , page_info.file_num
                                          , page_info.prev_page
                                          , page_info.next_next
                                          , page_info.link_num);
      }
    }
  }
      

  do
  {
    better_fat_file_num = 0;
    
    sector_info.bytes.flags = 0;
    
 // debug_printf ("fs_find_fat\n\r");
    for (uint32_t page = file_system_start_page ; page <= file_system_end_page ; page++) // page index - hopefully the memory device has less than 64k pages else mak this 32 bit
    {
      if ((page % flash_pages_per_sector == 0)
         || (sector_info.bytes.flags != ERASE_COUNT_KEY))
      {
        read_page_sector_head (page, &sector_info);

        if (sector_info.bytes.flags != ERASE_COUNT_KEY)
        {
          debug_printf ("Sector check failed @ page %u (sector %u)\n\r", page, page / flash_pages_per_sector);
          flash_driver_erase_sector_address (page_sector_address (page), false);                    // wipe sector
          sector_info.erase_count = 0;                                  // set_erase_count first
          sector_info.bytes.flags = ERASE_COUNT_KEY;
          write_page_sector_head (page, &sector_info); // rewrite erase count
          page += flash_pages_per_sector;
        }
      }
      
//      if (flash_sector_valid (page))
      {
        read_page_info (page, &page_info);

        if (page_info.file_num >= FIRST_FAT_FILE_NUM
          && page_info.file_num != 0xFFFFFFFF)   // found a fat page
        {
          if (better_fat_file_num)
          {
            if (page_info.file_num == better_fat_file_num)
            {
              saved_fat_page = page;
              better_fat_file_num++;
              if (better_fat_file_num == 0xFFFFFFFF)
                better_fat_file_num = FIRST_FAT_FILE_NUM;
              debug_printf ("found better_fat_file_num %hX @ %u\n\r", page_info.file_num, page);
            }
          }
          else
          {
            debug_printf ("found fat page %hX @ %u\n\r", page_info.file_num, page);
            saved_fat_page = page;
            better_fat_file_num = page_info.file_num + 1;
            if (better_fat_file_num == 0xFFFFFFFF)
              better_fat_file_num = FIRST_FAT_FILE_NUM;
          }
        }
      }
      
      fat_search_callback (page);
    }

    if (better_fat_file_num)
    {
      debug_printf ("using fat @ %u\n\r", saved_fat_page);
      fat->start_page = fs_check_file_chain (saved_fat_page);

      if (fat->start_page)
      {
        fat->current_page = fat->start_page;
        fat->current_offset = 0;
        read_page_info (fat->start_page, &fat->page_info);
    //    debug_printf (("fat start page = %u\n\r",fat.start_page));
        return (true);
      }
      else // fat file chain broken - wipe it look for a previous version.
      {    // we may have died while writing the fat so file fragments may remain - scan will clear it out
        fs_remove_file_chain (saved_fat_page);
      }
    }
  } while (better_fat_file_num);

  debug_printf ("fs_find_fat failed!\n\r");
  return (false);
}

//-------------------------------------------------------------------------------------
static bool fs_create_fat( FAT *fat, uint32_t fat_file_num )
{
  if (!fat)   return false;

  if (!flash_driver_init ())
  { // big problem
    media_error_callback       ("NO FLASH MEDIA AVAILABLE");
    return false;
  }
  
  //debug_printf ("fs_create_fat\n\r");
  fat->page_info.file_num   = fat_file_num;
  fat->page_info.prev_page  = 0;
  fat->page_info.next_page  = 0xFFFFFFFF;
  fat->page_info.link_num   = 0;

  fat->start_page           = fs_find_open_page();

  if (fat->start_page)
  {
    fat->current_page       = fat->start_page;
    fat->current_offset     = 0;
    write_page_info         (fat->current_page, &fat->page_info);
    
    return (true);
  }
  else
  {
    pseffs_error_callback (PSEFFS_ERROR_MEDIA_FULL);
  }
  
  debug_printf ("fs_create_fat failed!\n\r");
  
  return (false);
}

//-------------------------------------------------------------------------------------
static bool fs_advance_fat( FAT *fat )
{
  uint32_t next_link, next_page, fat_file_num;

  if (!fat)
    return false;

 // debug_printf ("fs_advance_fat\n\r");
  if (fat->page_info.next_page
    && fat->page_info.next_page != 0xFFFFFFFF)
  {
    fat_file_num          = fat->page_info.file_num;
    next_link             = next_link_num (fat->page_info.link_num);
    next_page             = fat->page_info.next_page;

    read_page_info (next_page, &fat->page_info);

    if ((fat->page_info.file_num == fat_file_num)
      && (fat->page_info.link_num == next_link))
    {
      fat->current_page     = next_page;      // set to next fat mem page
      fat->current_offset   = 0;            // reset file index
      return                (true);
    }
    
    debug_printf ("fs_advance_fat failed! 0x%X 0x%X 0x%X %u\n\r", fat->page_info.file_num
                                                                , fat->page_info.next_page
                                                                , fat->page_info.prev_page
                                                                , fat->page_info.link_num   );
    
    read_page_info (fat->current_page, &fat->page_info);
  }
  
  debug_printf ("fs_advance_fat returned to 0x%X 0x%X 0x%X %u\n\r", fat->page_info.file_num
                                                                  , fat->page_info.next_page
                                                                  , fat->page_info.prev_page
                                                                  , fat->page_info.link_num   );
  return (false);
}

//-------------------------------------------------------------------------------------
static bool  fs_fat_get_next_entry (FAT *fat, file_info_t *entry)
{
  if ( !entry || !fat )
    return false;

  if ((fat->current_offset + sizeof (file_info_t)) >= page_data_size (fat->current_page))
    if (!fs_advance_fat (fat))
      return false;

//  debug_printf ("fs_read_fat_entry\n\r");

  read_fat_entry (fat->current_page, fat->current_offset, entry);
    
  if (entry->file_num == 0xFFFFFFFF)
    return false;

  fat->current_offset += sizeof (file_info_t);

  return (true);
}

//-------------------------------------------------------------------------------------
static bool  fs_fat_put_next_entry (FAT *fat, file_info_t *entry, char *fat_name)
{
  if ( !entry || !fat )
    return false;
  
  if ((fat->current_offset + sizeof(file_info_t)) >= page_data_size (fat->current_page))
  {
    fat->page_info.next_page  = fs_find_open_page();
    
    if (fat->page_info.next_page)
    {
      write_page_info (fat->current_page, &fat->page_info); // this will over fat page info - should be identical exept for new next page, was 0xFFFFFFFF
      
      fat->page_info.prev_page  = fat->current_page;
      fat->current_page         = fat->page_info.next_page;
      fat->page_info.next_page  = 0xFFFFFFFF;
      fat->page_info.link_num   = next_link_num (fat->page_info.link_num);
      fat->current_offset       = 0;
   //   debug_printf(("Fat start page = %u, Fat Current page = %u\n\r",fat.start_page,fat.current_page));
   //   debug_printf_page_info (&fat.page_info);
      
      write_page_info (fat->current_page, &fat->page_info); // claim the new fat page
    }
    else // fat write current page failed
    {
      pseffs_error_callback (PSEFFS_ERROR_MEDIA_ERROR);
      return (false);
    }
  }
  
  write_fat_entry (fat->current_page, fat->current_offset, entry); // this will over flash - from FFFF to data - 0000
  
  fat->current_offset += sizeof (file_info_t);

// debug_printf ("fat current offset: %u\n\r", fat.current_offset));
  return (true);
}

//-------------------------------------------------------------------------------------
static void fs_recreate_fat (void)
{
  file_info_t fat_entry;
  
  FAT new_fat;
  
  RETRY_CREATE_FAT:
  
  fs_reset_fat();                                                       // reset current fat
  
  uint32_t new_fat_file_num = current_fat.page_info.file_num + 1;
  
  if (new_fat_file_num == 0xFFFFFFFF)
    new_fat_file_num = FIRST_FAT_FILE_NUM;
  
  if (fs_create_fat (&new_fat, new_fat_file_num))
  {
    while (fs_fat_get_next_entry (&current_fat, &fat_entry))
    {
      if (fat_entry.file_num)
      {
        if (!fs_fat_put_next_entry (&new_fat, &fat_entry, "NEW FAT"))
        {
          debug_printf ("fs_update_fat failed writing new fat entry!\n\r");
          fs_remove_file_fragments (new_fat_file_num);
          goto RETRY_CREATE_FAT;
        }
      }
    }
  }
  
  // seems all went well - remove old fat
  fs_remove_file_chain(current_fat.start_page);
  memcpy( &current_fat, &new_fat, sizeof(FAT) );
  flash_driver_save_quick_start_fat (current_fat.start_page);
}

//-------------------------------------------------------------------------------------
static void fs_fat_add (file_info_t *new_fat_entry)
{
  file_info_t fat_entry;
  int blank_entries = 0;
  
 // debug_printf ("fs_update_fat "));
 // debug_printf_file_info (file_info);
  
// try to find an old entry for sam file and clear it
  
  fs_reset_fat ();                                                      // reset current fat
  
  while (fs_fat_get_next_entry (&current_fat, &fat_entry))
  {
    if (fat_entry.file_num == 0)
      blank_entries++;
  }
  
  if (blank_entries > (page_data_size (current_fat.current_page)/ sizeof(file_info_t)))  // over half the page entries are blank write a new fat
  {
    fs_recreate_fat();
  }
  
  if (!fs_fat_put_next_entry (&current_fat, new_fat_entry, "CURR FAT"))
  {
    pseffs_error_callback (PSEFFS_ERROR_MEDIA_ERROR);
  }
}

/*-----------------------------------------------------------------------------------*/
static bool fs_fat_remove_num (uint32_t file_num)
{
  file_info_t   fat_entry;

 // debug_printf ("fs_fat_remove_num %u\n\r",file_num));

  fs_reset_fat();
  
  while (1)
  {
    if ((current_fat.current_offset + sizeof (file_info_t)) >= page_data_size (current_fat.current_page))
      if (!fs_advance_fat (&current_fat))
        return false;
    
    read_fat_entry (current_fat.current_page, current_fat.current_offset, &fat_entry);
  
    if (fat_entry.file_num == 0xFFFFFFFF)
      return false;
  
    if (fat_entry.file_num == file_num)
    {
      fat_entry.file_num = 0;
      
      if (fs_fat_put_next_entry (&current_fat, &fat_entry, "CURR FAT"))
        return (true);
    }
    
    current_fat.current_offset += sizeof (file_info_t);
  }
}

////-------------------------------------------------------------------------------------
//static void debug_printf_pseffs_error (pseffs_error_t error)
//{
//  switch (error)
//  {
//    case PSEFFS_ERROR_ILEGALE_CHAR:       debug_printf ("PSEFFS_ERROR_ILEGALE_CHAR\n\r");                 break;
//    case PSEFFS_ERROR_FILE_NAME_IN_USE:   debug_printf ("PSEFFS_ERROR_FILE_NAME_IN_USE\n\r");             break;
//    case PSEFFS_ERROR_FILE_NOT_FOUND:     debug_printf ("PSEFFS_ERROR_FILE_NOT_FOUND\n\r");               break;
//    case PSEFFS_ERROR_MEDIA_FULL:         debug_printf ("PSEFFS_ERROR_MEDIA_FULL\n\r");                   break;
//    case PSEFFS_ERROR_MEDIA_ERROR:        debug_printf ("PSEFFS_ERROR_MEDIA_ERROR\n\r");                  break;
//    case PSEFFS_ERROR_FILE_OPEN:          debug_printf ("PSEFFS_ERROR_FILE_OPEN\n\r");                    break;
//    case PSEFFS_ERROR_OUT_OF_MEM:         debug_printf ("PSEFFS_ERROR_OUT_OF_MEM\n\r");                   break;
//    case PSEFFS_ERROR_INVALID_SWITCH:     debug_printf ("PSEFFS_ERROR_INVALID_SWITCH\n\r");               break;
//    case PSEFFS_ERROR_EOF:                debug_printf ("PSEFFS_ERROR_EOF\n\r");                          break;
//    case PSEFFS_ERROR_NO_PERMISSION:      debug_printf ("PSEFFS_ERROR_NO_PERMISSION\n\r");                break;
//    case PSEFFS_ERROR_OS_LOCK:            debug_printf ("PSEFFS_ERROR_OS_LOCK\n\r");                      break;
//    case PSEFFS_ERROR_HANDLE:             debug_printf ("PSEFFS_ERROR_HANDLE\n\r");                       break;
//    default:                              debug_printf ("Unknown error number %u\n\r", error);            break;
//  }
//}

//-------------------------------------------------------------------------------------
void fs_scan_media (void)
{
  uint32_t      page;//, dead_pages = 0;
  page_info_t   page_info;
  file_info_t   fat_entry;
  bool linked   = false;
  char uppercase_fname[MAX_FNAME_SIZE];

  if (!flash_driver_init ())
  { // big problem
    media_error_callback ("NO FLASH MEDIA AVAILABLE");
    return;
  }
  
  //printf("Scanning media from %u to %u\n\r",file_system_start_page, file_system_end_page);
  scan_start_callback (file_system_start_page, file_system_end_page);

  fs_reset_fat ();
  
  while (fs_fat_get_next_entry (&current_fat, &fat_entry))
  {
    if (fat_entry.file_num
      && fat_entry.file_num != 0xFFFFFFFF)
    {
      if (!fs_format_fname (uppercase_fname,fat_entry.name))
      {
        fs_fat_remove_num     (fat_entry.file_num);
        fs_reset_fat          ();
        continue;
      }
      
      if (!fs_check_file_chain (fat_entry.start_page))
      {
        //printf("Integrity check for file %u %s failed - removing\n\r", file_info.file_num, file_info.name );
        scan_integrity_fail_callback  (fat_entry.file_num,uppercase_fname);
        fs_remove_file_chain          (fat_entry.start_page);
        fs_remove_file_fragments      (fat_entry.file_num);
        fs_fat_remove_num             (fat_entry.file_num);
        
        fs_reset_fat ();
      }
    }
  }
  
  for (page = file_system_start_page; page <= file_system_end_page; page++)
  {
    linked = false;
    
//    if (flash_sector_valid (page))
    {
      read_page_info (page, &page_info); // get page info
      
      if (page % 1000 == 0)
      {
        scan_callback (page);
      }

      if (page_info.file_num
        && page_info.file_num != 0xFFFFFFFF)
      {
        if (page_info.file_num >= 1
          && page_info.file_num < FIRST_FAT_FILE_NUM)
        {
//          if ( page_info.file_num == 0xFFFFFFFF ) // dead_page
//          {
//            dead_pages++;
//            //printf("Dead page - %u\n\r",page);
//            scan_dead_page_callback(page);
//          }
//          else
//          {
        
          fs_reset_fat ();
        
          while (!linked && fs_fat_get_next_entry (&current_fat, &fat_entry))
          {
            if (fat_entry.file_num == page_info.file_num)
            {
              if (fat_entry.start_page == page)
              {
                linked = true;
                    //printf ( "File %u %s, Page @ %u - link number %u\n\r", file_info.file_num, file_info.name, page, page_info.link_num );
                scan_file_info_callback (fat_entry.file_num,fat_entry.name,page,page_info.link_num);
              }
              else
              {
                read_page_info   (fat_entry.start_page, &page_info);

                while (page_info.next_page && page_info.next_page != 0xFFFFFFFF && (page_info.file_num == fat_entry.file_num))
                {
                  if (page_info.next_page == page)
                  {
                    linked = true;
                    read_page_info   (page_info.next_page, &page_info);
                    //printf ( "File %u %s, Page @ %u - link number %u\n\r", file_info.file_num, file_info.name, page, page_info.link_num );
                    scan_file_info_callback (fat_entry.file_num,fat_entry.name,page,page_info.link_num);
                    break; // from while ( page_info.next_page )
                  }
                  else
                  {
                    read_page_info   (page_info.next_page, &page_info); // get prev info
                  }
                }
              }
            }
          }
        }
        else // fat page
        if (page_info.file_num >= FIRST_FAT_FILE_NUM) // fat page
        {
          fs_reset_fat ();

          do
          {
            if (current_fat.current_page == page)
            {
              linked = true;
              break;
            }
          } while (fs_advance_fat (&current_fat));
        }
        else // fatbak page
        {
          // ?????????
        }

        if (!linked)
        {
  //        if ( page_info.file_num == 0xFFFFFFFF )
  //        {
  //          //printf("Dead page found @ %u\n\r", page);
  //          scan_dead_page_callback(page);
  //          fs_free_page (page);
  //          //printf("Page erased\n\r");
  //          scan_page_erased_callback(page);
  //        }
  //        else
          {
            //printf("Fragment found of File num %u, @ page %u\n\r",page_info.file_num, page);
            debug_printf ("Fragment found of File num %u, @ page %u\n\r",page_info.file_num, page);
            scan_fragment_callback (page_info.file_num,page);
            fs_free_page (page);
            //printf("Page erased\n\r");
            scan_page_erased_callback (page);
          }
        }
      }
    }
  }

  //printf("scanning done - %u dead pages found\n\r", dead_pages);
//  scan_end_callback(dead_pages);
  scan_end_callback (0);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//void flash_test(void)
//{
//  unsigned int i;
//  char temp_buf[20];
//
//  memset(temp_buf,0,sizeof(temp_buf));
//
//  for ( i = 0; i < file_system_end_page; i++ )
//  {
//    sprintf(temp_buf,"I am page %u", i );
//    Write_Flash_Data(i, 77, temp_buf, sizeof(temp_buf) );
//    DEBUG(("writing @ %u - %s\n\r",i,temp_buf));
//  }
//
//  for ( i = 0; i < file_system_end_page; i++ )
//  {
//    Read_Flash(i, 77, temp_buf, sizeof(temp_buf) );
//
//    DEBUG(("reading %u - %s\n\r",i, temp_buf));
//  }
//}

//-------------------------------------------------------------------------------------
#ifdef FS_FILEINFO
void fs_fileinfo (uint32_t *num_files, uint32_t *used_size, uint32_t *fat_size, uint32_t *sys_size )
{
  file_info_t spare_info;

  *num_files = 0;
  *used_size = 0;
  *fat_size = 0;

  //debug_printf ("fs_fileinfo "));
  fs_reset_fat ();

  while (fs_read_fat_entry (&spare_info))
  {
    *fat_size += sizeof(file_info_t);

    if ((strlen (spare_info.name)) && (spare_info.file_num != 0))
    {
      *num_files += 1;
      *used_size += spare_info.file_size;
    }
  }

  *sys_size = (uint32_t)( (file_system_end_page - file_system_start_page) + 1 ) * (uint32_t)page_data_size;
  //debug_printf (("-> num_files %u, files_size %u, fat_size %u, f_sys_size %u\n\r", *num_files, *used_size, *fat_size, *sys_size));
}
#endif

//-------------------------------------------------------------------------------------
//unsigned long fs_filesystem_info (unsigned long *total_size, unsigned int *num_files, unsigned long *files_size, char *min_wear_level_percent, char *max_wear_level_percent)
//{
//  unsigned int good_pages;
//  unsigned long available_bytes;
//
//  good_pages = fs_good_pages (min_wear_level_percent, max_wear_level_percent);
//  *total_size = (unsigned long) good_pages * DATA_SIZE;
//  fs_fat_fileinfo (num_files, files_size);
//  available_bytes = *total_size - *files_size;
//  return (available_bytes);
//}

//-------------------------------------------------------------------------------------
//void fs_print_fat(void)
//{
//  uint32_t page;

//  file_info_t file_info;
//  page_info_t page_info;

//  if ( fat.start_page )
//  {
//    fat.current_offset = 0;        // fat must always start on the first char of the page

//    if (fat.current_page != fat.start_page)
//    {
//      fat.current_page = fat.start_page;  // reset fat to start
//      fat.buffer = file_buffer_p[0];
//    
//      read_fat();
//    }

//    page = fat.start_page;
//    printf ("FAT start page: %u\n\r", fat.start_page);

//    do
//    {
//      read_page_info  (page,(uint8_t *) &page_info);

//      printf("Page %u: Wearlevel %u, file_num %u, prev_page %u, next_page %u, link_num %u\n\r",
//                                                                    page, Read_Erase_Count(page), 
//                                                                      page_info.file_num, page_info.prev_page,
//                                                                        page_info.next_page, page_info.link_num );
//      while( (fat.current_page == page) && (fat.buffer[fat.current_offset] != FAT_EOF) )
//      {
//        if ( fs_read_fat_entry(&file_info) )
//        {
//          printf("File num %u; Name %s; Start %u; Start Offset %u; Size %u\n\r",
//                                                                        file_info.file_num,
//                                                                          file_info.name,
//                                                                            file_info.start_page,
//                                                                              file_info.start_offset,
//                                                                                file_info.file_size );
//        }
//      }

//      page = fat.current_page;
//    } while( fat.buffer[fat.current_offset] != FAT_EOF );
//  }
//  else
//  {
//    printf ("ERROR - FAT not initialized\n\r");
//  }
//}

//-------------------------------------------------------------------------------------
//void fs_print_sector(uint32_t sector)
//{
//  page_info_t   page_info;
//  uint32_t    start_page,end_page;
//  uint8_t     address[4], c;

//  //debug_printf ("print_fs_sector %u\n\r", sector));
//  get_flash_sector_boundry(sector,&start_page,&end_page);

//  do
//  {
//    int i;

//    read_page_info  (start_page,(uint8_t *) &page_info);

//    printf("Page %u: Wearlevel %u, file_num %u, prev_page %u, next_page %u, link_num %u\n\r",
//      start_page, Read_Erase_Count(start_page), 
//        page_info.file_num, page_info.prev_page,
//          page_info.prev_page, page_info.link_num );

//    if ( page_info.file_num == 0 )
//    {
//      printf( "Page available\n\r" );
//    }
//    else if ( page_info.file_num > 0 && page_info.file_num <= 2 )
//    {
//      printf( "FAT page\n\r" );
//    }
//      
//    setupFlashAddress(address,start_page, FS_DATA_IDX);
//    (void)sendFlashCommand(READ_CONTINUOUS_COMMAND,address); // READ THE FLASH DIRECTLY

//    for ( i = 0; i < page_data_size; i++ )
//    {
////      printf("0x%02X",flash_inb());
//      c = spi_read_byte();
//      if (fs_isprint (c)) {printf ("%c", c);}
//      else             {printf ("(%02X)", (uint32_t)c);}
//    }

//    deselect_flash();

//    printf("\n\n\n\r");
//    start_page++;
//    //poll_callback();
//  } while ( start_page <= end_page );
//}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
