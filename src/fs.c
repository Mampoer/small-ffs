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
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//-------------------------------------------------------------------------------------
void DEBUG_printf  ( const char *ptr, ...    );

//-------------------------------------------------------------------------------------
#define debug_printf(x)
//#define debug_printf(x)  DEBUG_printf x 
#define fs_printf(...)   //term_printf(__VA_ARGS__)
#define debug_printf_file_header(x);
#define debug_printf_page_header(x,y,z);

//-------------------------------------------------------------------------------------
//#include "uart.h"
//#include "proto.h"
//#define debug_printf(x) DEBUG_printf x 

//-----------------------------------------------------------------------------------
// Debug functions
//-----------------------------------------------------------------------------------
//static void debug_printf_file_header (file_info_t *header)
//{
//  debug_printf (("file_num: %u, name: %s, start_page: %u, start_offset: %u, file_size: %u\n\r",
//  header->file_num, header->name, header->start_page, header->start_offset, header->file_size));
//}

////-----------------------------------------------------------------------------------
//static void debug_printf_page_header (uint16_t page, const page_info_t *header, char *filename)
//{
//  debug_printf (("page: %4hu, file_num: %4hX, prev_page: %4hu, next_page: %4hu, link_num: %4hu, %s\n\r",
//  page, header->file_num, header->prev_page, header->next_page, header->link_num, filename ));
//}

//-------------------------------------------------------------------------------------
static void     debug_printf_pseffs_error   (uint8_t error);

//-------------------------------------------------------------------------------------
struct file_list {
  FS_FILE   *fp;
  struct file_list *next;
};

struct file_list * first_file_list_item = NULL;

//-------------------------------------------------------------------------------------
FAT         current_fat             = {{0,0,0,0},0,0,0,NULL};

uint8_t     pseffs_error            = 0;

FS_FILE     *putchar_f              = NULL;

uint16_t    next_file_num           = 1;

//uint8_t     dead_page_revive_count  = 0;

uint16_t    start_search_page       = 0;

uint32_t    wear_threshold          = WEAR_THRESHOLD;

//-------------------------------------------------------------------------------------
// FILE FUNCTIONS
//-------------------------------------------------------------------------------------
static FS_FILE  *fs_get_file_mem          (void);
static uint16_t fs_find_open_page         (void);
static void     fs_load_file              (FS_FILE *fp);
static bool     fs_advance_file           (FS_FILE *fp);
static void     fs_free_file_mem          (FS_FILE *fp);
static bool     fp_valid                  (FS_FILE *fp);
static void     fs_remove_file_chain      (uint16_t page);
static void     fs_remove_file_fragments  (uint16_t file_num);
static bool     fs_file_is_open           (char *fname);
static bool     fs_format_fname           (char *fname_buf, const char *fname);
static bool     fs_init_new_file          (char *uppercase_fname, FS_FILE *fp);
static bool     fs_find_file_info         (char *uppercase_fname, FS_FILE *fp);
static bool     fs_find_file_num          (uint16_t filenum, FS_FILE *fp);
static bool     fs_write_page             (uint16_t page, const page_info_t *header, const uint8_t *dat, char *filename);
static void     fs_erase_page             (uint16_t page);
static bool     fs_advance_header         (FS_FILE *fp);

static bool     fs_reverse_header         (FS_FILE *fp);

//static uint16_t   rebuild_page_chain        (uint16_t end_page);

//-------------------------------------------------------------------------------------
// FAT FUNCTIONS
//-------------------------------------------------------------------------------------
static bool     fs_init_fat               (FAT *fat);
static bool     fs_find_fat               (FAT *fat);
static bool     fs_create_fat             (FAT *fat, uint16_t fat_file_num);
static void     fs_reset_fat              (void);
static int32_t  fs_fat_file_search        (char *fname);
static bool     fs_update_fat             (file_info_t *file_header);
static bool     fs_fat_remove_num         (uint16_t file_num);
static bool     fs_fat_remove             (char *file_name);
static bool     fs_read_fat_data          (FAT *fat, uint8_t *data, uint16_t len);
static bool     fs_write_fat_data         (FAT *fat, uint8_t *data, uint16_t len, char *fat_name);

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
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
#define page_info_size            sizeof(page_info_t)
#define page_header_offset(page)  (sector_info_size + ((page % flash_pages_per_sector) * flash_page_size))
#define page_data_offset(page)    (sector_info_size + ((page % flash_pages_per_sector) * flash_page_size) + page_info_size)
#define page_data_size            (flash_page_size - page_info_size)

//-------------------------------------------------------------------------------------
static void Read_Sector_Info(uint16_t sector_num, sector_info_t *sector_info)
{
  if ( sector_info )
  {
    flash_driver_set_flash_read(sector_num, 0);
    flash_driver_read((uint8_t *)sector_info, sizeof(sector_info_t));
    flash_driver_read_release();
  }
}

//-------------------------------------------------------------------------------------
static void Write_Sector_Info(uint16_t sector_num, sector_info_t *sector_info)
{
  if ( sector_info )
  {
    flash_driver_set_flash_write(sector_num, 0);
    flash_driver_write((uint8_t *)sector_info, sizeof(sector_info_t));
    flash_driver_write_release();
  }
}

//-------------------------------------------------------------------------------------
static void read_page_header ( uint16_t page_number, page_info_t *header )
{
  if ( header)
  {
    flash_driver_set_flash_read(sector_number(page_number), page_header_offset(page_number));
    flash_driver_read((uint8_t *)header, page_info_size);
    flash_driver_read_release();
  }
}

//-------------------------------------------------------------------------------------
static void read_page_data ( uint16_t page_number, uint8_t *data )
{
  if ( data )
  {
    flash_driver_set_flash_read(sector_number(page_number), page_data_offset(page_number));
    flash_driver_read(data, page_data_size);
    flash_driver_read_release();
  }
}

//-------------------------------------------------------------------------------------
static void read_page ( uint16_t page_number, page_info_t *header, uint8_t *data )
{
  if ( header && data )
  {
    flash_driver_set_flash_read(sector_number(page_number), page_header_offset(page_number));
    flash_driver_read((uint8_t *)header, page_info_size);
    flash_driver_read(data, page_data_size);
    flash_driver_read_release();
  }
}

//-------------------------------------------------------------------------------------
// File Systems Functions
//-------------------------------------------------------------------------------------
////-------------------------------------------------------------------------------------
//void shutdown_fs(void)
//{
//  int i;

//  debug_printf (("shutdown_fs\n\r"));
//  fs_fcloseall();

//  for ( i = 0; i < (2+MAX_OPEN_FILES); i++ )
//  {
//    if ( file_buffer_p[i] ) free(file_buffer_p[i]);
//  }
//}

//-------------------------------------------------------------------------------------
FS_FILE *fs_fopen (const char *filename, uint8_t mode)
{
  FS_FILE *fp = NULL;
  
  if ( fs_mutex_take() )
  {
    char uppercase_fname[MAX_FNAME_SIZE];

    //debug_printf (("fopen %s in mode %c\n\r", filename, mode));  //tvb

    pseffs_error = 0;

    fp = fs_get_file_mem();

    if ( fp )
    {
      if (fs_format_fname (uppercase_fname, filename))
      {
        //debug_printf (("uppercase filename %s \n\r ",uppercase_fname));  //tvb

		
        if (!fs_file_is_open(uppercase_fname))
        {
          if ( fs_find_file_info (uppercase_fname, fp) )
          {
            fp->mode_flag = fs_toupper(mode);

            switch (fp->mode_flag)
            {
              case 'R':
                fs_load_file(fp);
                break;
              case 'W':
                fp->file_header.file_num = 0;
                if ( fs_update_fat(&fp->file_header) ) // clear current fat entry
                {
                  fs_remove_file_chain(fp->file_header.start_page);
                  if ( fs_init_new_file (uppercase_fname,fp) )
                  {
                    break;
                  }
                }
    
                fs_free_file_mem(fp);
                fp = NULL;
                scan_fs_callback();
                break;
              case 'A':
                fp->current_page = fp->file_header.start_page;  // reset file to start page & offset
                fp->current_offset = fp->file_header.start_offset;
                fp->position = 0;
                read_page_header (fp->current_page, &fp->page_header);

                while ( fp->position < fp->file_header.file_size )
                {
                  int diff = fp->file_header.file_size - fp->position;
                  
                  if ( diff >= page_data_size )
                  {
                    if ( fs_advance_header (fp) )
                    {
                      fp->position += page_data_size;
                      fp->current_offset = 0;
                    }
                    else
                    {
                      break; // from while
                    }
                  }
                  else
                  {
                    fp->current_offset = diff;
                    fp->position += diff;
                  }
                }
                
                read_page_data (fp->current_page, fp->buffer);

                break;
              default:
                pseffs_error = PSEFFS_ERROR_INVALID_SWITCH;
                fs_free_file_mem (fp);
                break;
            }
          }
          else // file not found
          {
            fp->mode_flag = fs_toupper(mode);
    
            switch (fp->mode_flag)
            {
              case 'W':
              case 'A':
                if ( !fs_init_new_file (uppercase_fname,fp) )
                {
                  fs_free_file_mem(fp);
                  fp = NULL;
                }
                break;
              case 'R':
                fs_printf("fsopen PSEFFS_ERROR_FILE_NOT_FOUND %s\n\r",filename);
                pseffs_error = PSEFFS_ERROR_FILE_NOT_FOUND;
                fs_free_file_mem(fp);
                fp = NULL;
                break;
              default:
                pseffs_error = PSEFFS_ERROR_INVALID_SWITCH;
                fs_free_file_mem(fp);
                fp = NULL;
                break;
            }
          }
        }
        else
        {
          fs_free_file_mem(fp);
          fp = NULL;
        }
      }
      else
      {
        fs_free_file_mem(fp);
        fp = NULL;
      }
    }

    if (pseffs_error)
    {
      debug_printf_pseffs_error (pseffs_error);
    }
    
    fs_mutex_give();
  }
  
  return (fp);
}

//-------------------------------------------------------------------------------------
int fs_fgetc( FS_FILE *fp )
{
  int c = -1;
  
  if ( fs_mutex_take() )
  {
    if ( fp_valid(fp) )
    {
      if ( fp->mode_flag == 'R' )
      {
        if ( fp->position < fp->file_header.file_size )
        {
          c = (int)fp->buffer[fp->current_offset++];
          fp->position++;

          if ( fp->current_offset == page_data_size )
          {
            (void)fs_advance_file (fp);
          }
        }
        else
        {
          pseffs_error = PSEFFS_ERROR_EOF;
        }
      }
      else
      {
        pseffs_error = PSEFFS_ERROR_NO_PERMISSION;
        debug_printf (("fgetc failed in file %s with error ", file->file_header.name));
        debug_printf_pseffs_error (pseffs_error);
      }
    }
    
    fs_mutex_give();
  }

  if ( c < 0 ) debug_printf (("fgetc failed!\n\r"));
  
  return c;
}

//-------------------------------------------------------------------------------------
int fs_fgets( char *buf, int max_chars, FS_FILE *fp )
{
  int count = 0;
  int c;
  
 // debug_printf (("fgets (max %u characters) from file %s:\n\r", max_chars, file->file_header.name));
  while ( (count < max_chars) && ((c = fs_fgetc(fp)) >= 0) )
  {
    buf[count++] = c;
   // if (fs_isprint (c))  {debug_printf (("%c", c));}
   // else              {debug_printf (("(0x%02bX)", c));}
    if ( c == '\n' )
    {
      break;
    }
  }
  if ( count < max_chars ) // terminate if enough buf space
    buf[count] = '\0';
  //debug_printf (("\n\r"));
  return(count);
}

//-------------------------------------------------------------------------------------
// This function will get a specified number of characters from the file and place
// them in the buffer, if it can't, it will fill up the remaining buffer spaces with
// zeros
void fs_fgetsposn (char *buf, int num_chars, FS_FILE *fp, int start_pos)
{
  int count = 0;
  int c;
  
  if ( fp_valid(fp) )
  {
    //debug_printf (("fgetsposn %u characters from file %s at position %u:\n\r", num_chars, file->file_header.name, start_pos));
    if ( fp->mode_flag == 'R' )
    {
      fs_fsetpos (fp,start_pos);
      while ( count < num_chars && (c = fs_fgetc(fp)) >= 0 )
      {
        buf[count++] = c;
  //       if (fs_isprint (c))  {debug_printf (("%c", c));}
  //       else              {debug_printf (("(0x%02bX)", c));}
      }

      while (count < num_chars)
      {
        buf[count++] = 0;
      }
    }
   // debug_printf (("\n\r"));
  }
}

//-------------------------------------------------------------------------------------
static uint16_t next_link_num(uint16_t link_num)
{
  if ( link_num == 0xFFFE ) return 0;
  else return link_num + 1;
}

//-------------------------------------------------------------------------------------
static uint16_t previous_link_num(uint16_t link_num)
{
  if ( link_num == 0 ) return 0xFFFE;
  else return link_num - 1;
}

//-------------------------------------------------------------------------------------
bool  fs_fputc( int c, FS_FILE *fp )
{
  if ( fs_mutex_take() )
  {
    if (fp_valid(fp)) // check permission
    {
      if ( fp->mode_flag == 'W' || fp->mode_flag == 'A' )  //random access
      {
        if (fp->current_offset == page_data_size)
        {
          RETRY_FPUTC:
          fp->page_header.next_page = fs_find_open_page();  // get open page 

          if ( fp->page_header.next_page )                  // got an open page
          {    // last page is a problem!!!!
            if ( fs_write_page(fp->current_page, &fp->page_header, fp->buffer, fp->file_header.name) )
            {                                                 // wrote current page
              fp->page_header.prev_page = fp->current_page; // set new prev page
              fp->current_page = fp->page_header.next_page;
              fp->page_header.next_page = 0xFFFF;
              fp->page_header.link_num = next_link_num(fp->page_header.link_num);

              memset( fp->buffer,0xFF, page_data_size );
              fp->current_offset = 0;

              if ( fp->file_header.start_page == 0 )         // this is the first page of the file
              {                                                // dont have to worry about rebuild here
                fp->file_header.start_page = fp->page_header.prev_page;
              }
            }
            else // page write failed
            {
              fp->current_page = fp->page_header.next_page; // current page is no good so use open page as new current page
              fp->page_header.next_page = 0xFFFF;                  // - in fclose we will scan the file and rebuild the chain
              goto RETRY_FPUTC;
            } 
          }
          else  // file sys full - error critical at this stage
          {
            pseffs_error = PSEFFS_ERROR_MEDIA_FULL;
            debug_printf (("fputc failed in file %s with error ", file->file_header.name));
            debug_printf_pseffs_error (pseffs_error);
            fs_mutex_give();
            return(false);
          }
        }

        fp->buffer[fp->current_offset++] = c;
        fp->position++;
        fs_mutex_give();
        return(true);
      }
      else
      {
        pseffs_error = PSEFFS_ERROR_NO_PERMISSION;
        debug_printf (("fputc failed in file %s with error ", file->file_header.name));
        debug_printf_pseffs_error (pseffs_error);
      }
    }
    
    fs_mutex_give();
  }
  debug_printf (("fputc failed!\n\r"));
  return(false);
}

//-------------------------------------------------------------------------------------
int fs_fputs(const char *buf, FS_FILE *fp)
{
  int count = 0;

 // debug_printf (("fputs %s in file %s\n\r", buf, file->file_header.name));
  while ( buf[count] )
  {
    if ( fs_fputc(buf[count], fp) )
    {
      count++;
    }
    else
    {
      return( count );
    }
  }
  
  return( count );
}

//-------------------------------------------------------------------------------------
bool  fs_fclose(FS_FILE *fp)
{
  page_info_t header;
  uint16_t fnum;
  bool rebuild;

  if ( fs_mutex_take() )
  {
   // debug_printf (("fclose %s mode %c\n\r", fp->file_header.name, fp->mode_flag));
    if (fp_valid(fp))
    {
      if ( fp->mode_flag == 'W' || fp->mode_flag == 'A' )
      {
        fp->file_header.file_size = fp->position;                             // file header ready for writing to fat

        fnum = fp->file_header.file_num;

        RETRY_FCLOSE:

        if ( fs_write_page (fp->current_page, &fp->page_header, fp->buffer, fp->file_header.name) ) // write open file current page
        {                                                                     // write is good
          RETRY_FCLOSE_ROLL:

          rebuild = false;

          // ROLL BACK TO START
          while ( fp->page_header.prev_page )
          {
            read_page_header ( fp->page_header.prev_page, &header );          // read prev header into temp header

            if ( header.file_num == fnum )
            {                                                                 // previous header file num is good
              if ( header.next_page != fp->current_page )                     // check chain integrity
              {                                                               // previous page header needs correction
                header.next_page = fp->current_page;                          // fix error
                read_page_data (fp->page_header.prev_page, (uint8_t*)fp->buffer);      // read previous page data into file buffer
                do                                                            // try to write previous page with good forward pointer
                {                                                             // this messes up current header but we will fix when going forward
                  fp->page_header.prev_page = fs_find_open_page();            // find new space for previous page
                  if ( !fp->page_header.prev_page )                           // out of space
                  {                                                           // cant restore file chain without space so just wipe
                    fs_remove_file_fragments(fp->file_header.file_num);
                    fs_free_file_mem(fp);
                    if ( !pseffs_error )
                    {
                      pseffs_error = PSEFFS_ERROR_MEDIA_FULL;
                    }
                    debug_printf (("fclose error 1\n\r"));
                    debug_printf_pseffs_error (pseffs_error);
                    fs_mutex_give();
                    return(false);
                  }                                                           // got space
                  rebuild = true;
                } while ( !fs_write_page(fp->page_header.prev_page, &header, fp->buffer, fp->file_header.name) ); // redo untill success
              }
              fp->current_page = fp->page_header.prev_page;                   // update file header from temp header
              memcpy( (uint8_t*) &fp->page_header, (uint8_t*) &header, page_info_size );
            }
            else // previous page belongs to diffrent file - we lost backwards integrity
            {    // maybe try to rebuld the file by link number - for now we just wipe it
              fs_remove_file_fragments(fp->file_header.file_num);
              fs_free_file_mem(fp);
              if ( !pseffs_error )
              {
                pseffs_error = PSEFFS_ERROR_MEDIA_ERROR;
              }
              debug_printf (("fclose error 2\n\r"));
              debug_printf_pseffs_error (pseffs_error);
              fs_mutex_give();
              return(false);
            }
          } 

          // test if start page param valid - may have changed in a backward rebuild
          if ( fp->current_page != fp->file_header.start_page )
            fp->file_header.start_page = fp->current_page;

          if ( rebuild )
          {
            rebuild = false;

            while( fp->page_header.next_page && fp->page_header.next_page != 0xFFFF )
            {
              read_page_header ( fp->page_header.next_page, &header );        // get next header (lo)
              if ( header.file_num == fnum )
              {                                                               // farward header good
                if ( header.prev_page != fp->current_page )                   // check chain integrity
                {                                                             // next page header needs correction
                  header.prev_page = fp->current_page;                        // fix error
                  read_page_data (fp->page_header.next_page, (uint8_t*)fp->buffer);      // get page data
                  do
                  {
                    fp->page_header.next_page = fs_find_open_page();          // this mess up current header again so we will redo the entire process after forward fix
                    if ( !fp->page_header.next_page )                         // out of space
                    {
                      fs_remove_file_fragments(fp->file_header.file_num);
                      fs_free_file_mem(fp);
                      if ( !pseffs_error )
                      {
                        pseffs_error = PSEFFS_ERROR_MEDIA_FULL;
                      }
                      debug_printf (("fclose error 3\n\r"));
                      debug_printf_pseffs_error (pseffs_error);
                      fs_mutex_give();
                      return(false);
                    }
                    rebuild = true;
                  } while ( !fs_write_page(fp->page_header.next_page, &header, fp->buffer, fp->file_header.name) );
                }
                fp->current_page = fp->page_header.next_page;
                memcpy( (uint8_t*) &fp->page_header, (uint8_t*) &header, page_info_size );
              }
              else // lost forward integrity
              {
                fs_remove_file_fragments(fp->file_header.file_num);
                fs_free_file_mem(fp);
                if ( !pseffs_error )
                {
                  pseffs_error = PSEFFS_ERROR_MEDIA_FULL;
                }
                debug_printf (("fclose error 4\n\r"));
                debug_printf_pseffs_error (pseffs_error);
                fs_mutex_give();
                return(false);
              }
            }

            if ( rebuild )
            {
              goto RETRY_FCLOSE_ROLL;
            }
          }

          if ( !fs_update_fat(&fp->file_header) )
          {
            if ( !pseffs_error )
            {
              pseffs_error = PSEFFS_ERROR_MEDIA_ERROR;
            }
            debug_printf (("fclose failed with error 5\n\r"));
            debug_printf_pseffs_error (pseffs_error);
            fs_remove_file_chain(fp->current_page);
            fs_free_file_mem(fp);
            fs_mutex_give();
            return(false);
          }

          fs_free_file_mem(fp);
          fs_mutex_give();
          return(true);
        }
        else // failed to write last file data page
        {
          fp->current_page = fs_find_open_page();  // get new last page

          if ( fp->current_page )
          {
            goto RETRY_FCLOSE;
          }
          else // no space
          {
            fs_remove_file_fragments(fp->file_header.file_num);
            fs_free_file_mem(fp);
            if ( !pseffs_error )
            {
              pseffs_error = PSEFFS_ERROR_MEDIA_FULL;
            }
            debug_printf (("fclose error 7\n\r"));
            debug_printf_pseffs_error (pseffs_error);
            fs_mutex_give();
            return(false);
          }
        }
      }

      // reading
      fs_free_file_mem(fp);
      fs_mutex_give();
      return(true);
    }
    
    debug_printf (("fclose error no handle\n\r"));
    fs_mutex_give();
  }
  return(false);
}

//-------------------------------------------------------------------------------------
void fs_fcloseall(void)
{
  while ( first_file_list_item )
  {
    if ( first_file_list_item->fp )
      fs_fclose (first_file_list_item->fp);
  }
}

//-------------------------------------------------------------------------------------
bool fs_remove(const char *filename)
{
  if ( fs_mutex_take() )
  {
    FS_FILE *fp;
    char uppercase_fname[MAX_FNAME_SIZE];

    //debug_printf (("remove file %s\n\r", filename));

    fp = fs_get_file_mem();

    if ( fp )
    {
      if (fs_format_fname (uppercase_fname, filename))
      {
        if (!fs_file_is_open(uppercase_fname) )
        {
          if ( fs_find_file_info(uppercase_fname,fp))
          {
            if ( fs_fat_remove(uppercase_fname) )
            {
              fs_remove_file_chain(fp->file_header.start_page);
              fs_free_file_mem(fp);
              fs_mutex_give();
              return(true);
            }
          }
          fs_free_file_mem( fp );
        }
      }
    }
    
    fs_mutex_give();
  }
  return(false);
}

/*-----------------------------------------------------------------------------------*/
bool fs_remove_num(uint16_t filenum)
{
  if ( fs_mutex_take() )
  {
    FS_FILE *fp;

    //debug_printf (("remove file num %u\n\r", filenum));

    fp = fs_get_file_mem();

    if ( fp )
    {
      if ( fs_find_file_num(filenum,fp))
      {
        if ( fs_fat_remove_num(filenum) )
        {
          fs_remove_file_chain(fp->file_header.start_page);
          fs_free_file_mem(fp);
          fs_mutex_give();
          return(true);
        }
      }

      fs_free_file_mem( fp );
    }
    
    fs_mutex_give();
  }

  debug_printf (("remove %d failed!\n\r", filenum));
  return(false);
}

//-------------------------------------------------------------------------------------
bool fs_rename(const char *oldfile, const char *newfile)
{
  if ( fs_mutex_take() )
  {
    FS_FILE *fp;
    char uppercase_oldfile[MAX_FNAME_SIZE];
    char uppercase_newfile[MAX_FNAME_SIZE];

    //debug_printf (("rename file %s to %s\n\r", oldfile, newfile));

    fp = fs_get_file_mem();

    if ( fp )
    {
      if (fs_format_fname (uppercase_newfile, newfile))
      {
        if ( !fs_file_is_open(uppercase_newfile) )
        {
          if ( fs_find_file_info(uppercase_newfile,fp) )
          { // filename exists already
            pseffs_error = PSEFFS_ERROR_FILE_NAME_IN_USE;
            debug_printf_pseffs_error (pseffs_error);
            fs_free_file_mem(fp);
            fs_mutex_give();
            return(false);
          }

          fp->file_header.file_num = 0; // clear mem
          fp->file_header.name[0]  = 0; // clear mem
        }
        else
        {
          fs_free_file_mem(fp);
          fs_mutex_give();
          return(false);
        }
      }
      else // file name ileagle or open
      {
        fs_free_file_mem(fp);
        fs_mutex_give();
        return(false);
      }
      
      if (fs_format_fname (uppercase_oldfile, oldfile))
      {
        if ( !fs_file_is_open(uppercase_oldfile) )
        {
          if ( fs_find_file_info(uppercase_oldfile,fp) )
          {
            strcpy (fp->file_header.name, uppercase_newfile);
          
            if ( fs_update_fat(&fp->file_header) )
            {
              fs_free_file_mem( fp );
              fs_mutex_give();
              return(true);
            }
            else
            {
              fs_free_file_mem( fp );
              fs_mutex_give();
              return(false);
            }
          }
          fs_printf("fscopy PSEFFS_ERROR_FILE_NOT_FOUND %s %s\n\r",oldfile,newfile);    
          pseffs_error = PSEFFS_ERROR_FILE_NOT_FOUND;
          debug_printf_pseffs_error (pseffs_error);
          fs_free_file_mem(fp);
          fs_mutex_give();
          return(false);
        }
        else
        {
          fs_free_file_mem(fp);
          fs_mutex_give();
          return(false);
        }
      }
      else // file name ileagle
      {
        fs_free_file_mem(fp);
        fs_mutex_give();
        return(false);
      }
    }

    debug_printf (("rename failed - no mem available\n\r"));
    fs_mutex_give();
  }

  return(false);
}

//-------------------------------------------------------------------------------------
int32_t fs_access (const char *fname)
{
  int32_t ret = -1;
  
  if ( fs_mutex_take() )
  {
    char uppercase_fname[MAX_FNAME_SIZE];

    //debug_printf (("access %s\n\r", fname));
    if (fs_format_fname (uppercase_fname, fname))
    {
      ret = fs_fat_file_search (uppercase_fname);
    }
    
    fs_mutex_give();
  }
  
  return (ret);
}

//-------------------------------------------------------------------------------------
bool fs_fclip (const char *filename, uint32_t num_bytes)
{
  if ( fs_mutex_take() )
  {
    if ( num_bytes )
    {
      FS_FILE *fp;
      char uppercase_fname[MAX_FNAME_SIZE];

      //debug_printf (("fclip %u bytes from file %s\n\r", num_bytes, filename));

      fp = fs_get_file_mem();

      if ( fp )
      {
        if ( fs_format_fname (uppercase_fname, filename) )
        {
          if ( !fs_file_is_open( uppercase_fname ) )
          {
            if ( fs_find_file_info(uppercase_fname,fp) )
            {
              if (num_bytes >= fp->file_header.file_size)
              {
                fs_free_file_mem(fp);
                fs_mutex_give();
                return (fs_remove (uppercase_fname));
              }

              fs_load_file(fp);

            //  debug_printf (("start_page %u, start_offset %u, file_size %u\n\r", fp->file_header.start_page, fp->file_header.start_offset, fp->file_header.file_size));

              while (num_bytes >= page_data_size)
              {
                if ( fs_advance_header(fp) )
                {
                  num_bytes -= page_data_size;
                  fp->file_header.file_size -= page_data_size;
                  fp->file_header.start_page = fp->current_page;
                  fs_erase_page (fp->page_header.prev_page);
                }
                else
                { // file is corrupt as an african politician
                  fs_free_file_mem(fp);
                  fs_mutex_give();
                  return (fs_remove (uppercase_fname));
                }
              }

            //  debug_printf (("start_page %u, start_offset %u, file_size %u\n\r", fp->file_header.start_page, fp->file_header.start_offset, fp->file_header.file_size));

              if ( fp->current_offset + num_bytes >= page_data_size )
              {
                if ( fs_advance_header(fp) )
                {
                  fp->current_offset = (fp->current_offset + (uint16_t)num_bytes) - page_data_size;
                  fs_erase_page (fp->page_header.prev_page);
                }
                else
                { // file is corrupt as an african politician
                  fs_free_file_mem(fp);
                  fs_mutex_give();
                  return (fs_remove (uppercase_fname));
                }
              }
              else
              {
                fp->current_offset = fp->current_offset + (uint16_t)num_bytes;
              }

              //debug_printf (("start_page %u, start_offset %u, file_size %u\n\r", fp->file_header.start_page, fp->file_header.start_offset, fp->file_header.file_size));
       
              fp->file_header.start_page   = fp->current_page;
              fp->file_header.start_offset = fp->current_offset;
              fp->file_header.file_size   -= num_bytes;

              //debug_printf (("start_page %u, start_offset %u, file_size %u\n\r", fp->file_header.start_page, fp->file_header.start_offset, fp->file_header.file_size));

              if (fp->page_header.prev_page)
              { // update header in first file page
                fp->page_header.prev_page  = 0;
                read_page_data (fp->current_page, (uint8_t*)fp->buffer);

                while ( !fs_write_page (fp->current_page, &fp->page_header, fp->buffer, fp->file_header.name) )
                {
                  fp->current_page = fs_find_open_page();

                  if (fp->current_page)
                  {
                    fp->file_header.start_page = fp->current_page;
                  }
                  else
                  {
                    fs_remove_file_fragments(fp->file_header.file_num);
                    fp->file_header.file_num = 0;
                    fs_update_fat(&fp->file_header);
                    fs_free_file_mem(fp);
                    fs_printf("fsclip PSEFFS_ERROR_MEDIA_ERROR %s %s\n\r",filename);    
                    pseffs_error = PSEFFS_ERROR_MEDIA_ERROR;
                    debug_printf_pseffs_error (pseffs_error);
                    fs_mutex_give();
                    return (false);
                  }
                }
                
                // technicaly the back pointer for the next page will be incorrect if the first write failed - but fuckit for now
              }

             // debug_printf (("start_page %u, start_offset %u, file_size %u\n\r", fp->file_header.start_page, fp->file_header.start_offset, fp->file_header.file_size));

              if ( fs_update_fat(&fp->file_header) )
              {
                fs_free_file_mem(fp);
                fs_mutex_give();
                return (true);
              }
  //            else
  //           {
  //             // free_file_chane(fp)
  //             free_file_mem( fp );
  //           }
            }
            else
            {
              fs_printf("fsclip PSEFFS_ERROR_FILE_NOT_FOUND %s %s\n\r",filename);    
              pseffs_error = PSEFFS_ERROR_FILE_NOT_FOUND;
              debug_printf_pseffs_error (pseffs_error);
            }
          }
        }

        fs_free_file_mem(fp);
      }
    }
    
    fs_mutex_give();
  }

  debug_printf (("fclip failed!\n\r"));
  return (false);
}

//-------------------------------------------------------------------------------------
//void fsetpos (FS_FILE *fp, uint32 position)
//{
//  uint16_t loaded_page;
//  uint32 difference;
//
//  debug_printf (("fsetpos in %s to %u\n\r", fp->file_header.name, position));
//  loaded_page = fp->current_page;
//  
//  if (position > fp->file_header.file_size)
//  { // beyond file end - just set to end of file
//    position = fp->file_header.file_size;
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
//      fs_advance_header (fp);
//    }
//    if (difference + fp->current_offset >= page_data_size)
//    {
//      difference -= (page_data_size - fp->current_offset);
//      fp->position += (page_data_size - fp->current_offset);
//      fp->current_offset = 0;
//      fs_advance_header (fp);
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
//      fs_reverse_header (fp);
//    }
//    if (difference > fp->current_offset)
//    {
//      difference -= fp->current_offset;
//      fp->position -= fp->current_offset;
//      //Can set offset to data size here, 'cause it'll get decremented below
//      fp->current_offset = page_data_size;      
//      fs_reverse_header (fp);
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
void fs_fsetpos (FS_FILE *fp, uint32_t position)
{
 // debug_printf (("fsetpos in %s to %u\n\r", fp->file_header.name, position));

  if ( fs_mutex_take() )
  {
    if ( fp->mode_flag == 'R' ) /*enable seek on read and write*/
    {
    
      if (position > fp->file_header.file_size)
      { // beyond file end - just set to end of file
        position = fp->file_header.file_size;
      }
      
      if (position == fp->position)
      { // Already in the right position
        fs_mutex_give();
        return;
      }
    
      if (position > fp->position)
      { // Need to step forward
        while ( fp->position < position )
        {
          
          fp->current_offset++;
          fp->position++;
    
          if ( fp->current_offset == page_data_size )
          {
            if ( fs_advance_header (fp) )
              fp->current_offset = 0;
            else
            {
              read_page_data(fp->current_page, fp->buffer);
              fs_mutex_give();
              return;
            }
          }
      
        }
      }
      else
      {
        while ( fp->position > position )
        {
          if ( fp->current_offset == 0 )
          {
            if ( fs_reverse_header (fp) )
              fp->current_offset = page_data_size;
            else
            {
              read_page_data(fp->current_page, fp->buffer);
              fs_mutex_give();
              return;
            }
          }
      
          fp->current_offset--;
          fp->position--;
        }
      }
    
      read_page_data(fp->current_page, fp->buffer);
    }
    
    fs_mutex_give();
  }
}

//-------------------------------------------------------------------------------------
//uint32 fseekend (FS_FILE *fp)
//{
//  uint32 file_size = 0;
//
//  debug_printf (("fseekend of file %s\n\r", fp->file_header.name));
//  fsetpos (fp, fp->file_header.file_size);
//
//  return (fp->position);
//}

//-------------------------------------------------------------------------------------
uint32_t fs_fseekend (FS_FILE *fp)
{
 // debug_printf (("fseekend of file %s\n\r", fp->file_header.name));

  if ( fs_mutex_take() )
  {
    while ( fp->position < fp->file_header.file_size )
    {
      if ( fp->current_offset == page_data_size )
      {
        if ( fs_advance_header (fp) )
          fp->current_offset = 0;
        else
        {
          read_page_data(fp->current_page, (uint8_t*)fp->buffer);
          fs_mutex_give();
          return (fp->position);
        }
      }

      fp->current_offset++;
      fp->position++;
    }

    read_page_data(fp->current_page, (uint8_t*)fp->buffer);
    fs_mutex_give();
    return (fp->position);
  }

  return 0;
}

//-------------------------------------------------------------------------------------
void fs_freverse (FS_FILE *fp, uint32_t numchars)
{
  //debug_printf (("freverse %u characters in file %s\n\r", numchars, fp->file_header.name));
  fs_fsetpos( fp, fp->position - numchars);
}

//-------------------------------------------------------------------------------------
#ifdef FILE_PRINTF
static int file_putchar(int c)
{
  if ( putchar_f )
  {
    fs_fputc( c, putchar_f );
  }

  return ( c );
}
#endif

//-------------------------------------------------------------------------------------
#ifdef FILE_PRINTF
void file_printf( FS_FILE *f, const char *ptr, ... )
{
  extern int (*putchar_func_pointer)(int);
  int (*putchar_ptr_save)(int) = putchar_func_pointer;
  __va_list var_ptr;
  va_start(var_ptr,ptr);
  putchar_func_pointer = file_putchar;
  putchar_f = f;
  (void)vprintf (ptr, var_ptr);
  va_end(var_ptr);
  putchar_func_pointer = putchar_ptr_save;
  putchar_f = 0;
}
#endif

//-------------------------------------------------------------------------------------
// File Systems IO Functions
//-------------------------------------------------------------------------------------
//static void fs_kill_page (uint16_t page)
//{
//  //debug_printf (("fs_kill_page %u\n\r", page));
//  Erase_Page(page);
//}

//-------------------------------------------------------------------------------------
static void fs_erase_page (uint16_t page)
{
  page_info_t header = {0,0,0,0};
  debug_printf (("fs_erase_page %u\n\r", page));
  flash_driver_set_flash_write(sector_number(page), page_header_offset(page));
  flash_driver_write((uint8_t *)&header, page_info_size);
  flash_driver_write_release();
}

////-------------------------------------------------------------------------------------
//static void fs_erase_page (uint16_t page)
//{
//  page_info_t header;
//  
//  debug_printf (("fs_erase_page %u\n\r", page));
//  read_page_header (page, &header);     // read header from flash (8 bytes)
//  header.file_num = 0;                          // clear file number (make page available)
////  Write_Buffer (FS_PAGE_IDX, &header, FS_PAGE_SIZE);  // update page info in buffer
//  write_page_header(page, &header);            // over write file num to zero
//}

//-------------------------------------------------------------------------------------
static uint16_t fs_check_file_chain( uint16_t file_page )
{
  page_info_t page_header;
  uint16_t file_num, start_page, link_num, curr_page, last_page;

  //debug_printf (("check_file_chain from %u\n\r", file_page));

  if ( file_page < flash_first_page || file_page > flash_last_page )
  {
    debug_printf (("invalid page\n\r"));
    return(0);
  }

  read_page_header (file_page, &page_header);

  if ( page_header.file_num == 0 )
  { // this page is not a valid file page
    debug_printf (("page is not a file page\n\r"));
    return(0);
  }

  file_num    = page_header.file_num;
  link_num    = page_header.link_num;
  curr_page   = file_page;
  last_page   = curr_page;
  start_page  = curr_page;

  //debug_printf(("checking integrity for file %u\n\r",file_num)); 

  while ( page_header.prev_page )   // backward
  {
    curr_page = page_header.prev_page;
   // debug_printf(("reading page %u\n\r",curr_page));
    read_page_header (curr_page, &page_header);

    if (file_num != page_header.file_num )
    {
      debug_printf(("failed with incorrect file_num %u\n\r",page_header.file_num));
      return(0);
    }
    if (link_num != next_link_num(page_header.link_num))
    {
      debug_printf(("failed with incorrect link_num %u, should be %u\n\r",page_header.link_num,(page_header.link_num+1)));
      return(0);
    }
    if (page_header.next_page != last_page)
    {
      debug_printf(("failed with incorrect next_page %u, should be %u\n\r",page_header.next_page, last_page));
      return(0);
    }

    link_num    = previous_link_num(link_num);
    last_page   = curr_page;
    start_page  = curr_page;
  }

  if ( curr_page != file_page )
  {
    read_page_header (file_page, &page_header);
  }

  link_num    = page_header.link_num;
  curr_page   = file_page;
  last_page   = curr_page;

  while ( page_header.next_page && page_header.next_page != 0xFFFF )   // forward
  {
    curr_page = page_header.next_page;
    //debug_printf(("reading page %u\n\r",curr_page));
    read_page_header (curr_page, &page_header);

    if (file_num != page_header.file_num )
    {
      debug_printf(("failed with incorrect file_num %u\n\r",page_header.file_num));
      return(0);
    }
    if (link_num != previous_link_num(page_header.link_num))
    {
      debug_printf(("failed with incorrect link_num %u, should be %u\n\r",page_header.link_num,(page_header.link_num-1)));
      return(0);
    }
    if (page_header.prev_page != last_page)
    {
      debug_printf(("failed with incorrect prev_page %u, should be %u\n\r",page_header.prev_page, last_page));
      return(0);
    }

    link_num    = next_link_num(link_num);
    last_page   = curr_page;
  }

  return(start_page);
}

//-------------------------------------------------------------------------------------
static void fs_remove_file_fragments( uint16_t file_num )
{
  page_info_t page_header;
  uint16_t page;                       // page index - hopefully the memory device has less than 64k pages else mak this 32 bit

  debug_printf (("fs_remove_file_fragments for file %hX\n\r", file_num));
  for ( page = flash_first_page ; page <= flash_last_page ; page++ )
  {
    read_page_header (page, &page_header);
    if ( page_header.file_num == file_num )   // found a file page
    {
      debug_printf(("found file %hX fragment @ page %u\n\r",file_num,page));
      fs_erase_page (page);
    }
  }
}

//-------------------------------------------------------------------------------------
static void fs_remove_file_chain(uint16_t page)
{
  page_info_t page_header;
  uint16_t file_num, curr_page = page;

  //debug_printf (("fs_remove_file_chain from %u\n\r", page));

  if ( page < flash_first_page || page > flash_last_page )
  {
    debug_printf (("fs_remove_file_chain - invalid page %u\n\r", page));
    return;
  }

  read_page_header (curr_page, &page_header);

  if ( page_header.file_num == 0 || page_header.file_num == 0xFFFF )
  { // this page is free
   // debug_printf (("page was free\n\r"));
    return;
  }

  file_num = page_header.file_num;

  while (page_header.prev_page                      &&
        (page_header.prev_page >= flash_first_page) &&
        (page_header.prev_page <= flash_last_page)    ) // backward
  {
    curr_page = page_header.prev_page;
    read_page_header (curr_page, &page_header);
    if (file_num == page_header.file_num)
    {
      debug_printf (("fs_remove_file_chain backwards %u\n\r", curr_page));
      fs_erase_page (curr_page);
    }
    else
    {
      fs_remove_file_fragments(file_num);
      return;
    }
  }

  if ( curr_page != page )
  {
    curr_page = page;
    read_page_header (curr_page, &page_header);
  }

  while (page_header.next_page                      &&
         page_header.next_page != 0xFFFF            &&
        (page_header.next_page >= flash_first_page) &&
        (page_header.next_page <= flash_last_page)    ) // forward
  {
    curr_page = page_header.next_page;
    read_page_header (curr_page, &page_header);
    if (file_num == page_header.file_num)
    {
      debug_printf (("fs_remove_file_chain forward %u\n\r", curr_page));
      fs_erase_page (curr_page);
    }
    else
    {
      fs_remove_file_fragments(file_num);
      return;
    }
  }

  debug_printf (("fs_remove_file_chain current %u\n\r", page));
  fs_erase_page (page);
}

//-------------------------------------------------------------------------------------
static bool  fs_advance_file(FS_FILE * fp)
{
  uint16_t next_link, next_page;

 // debug_printf (("advance_file %s\n\r", fp->file_header.name));
  if (fp->page_header.next_page && fp->page_header.next_page != 0xFFFF)
  {
    next_link = next_link_num(fp->page_header.link_num);
    next_page = fp->page_header.next_page;

    read_page_header(next_page, &fp->page_header);

    if ( (fp->page_header.file_num == fp->file_header.file_num) &&
         (fp->page_header.link_num == next_link) )
    {
      fp->current_page = next_page;       // set to next flash page
      fp->current_offset = 0;             // reset file index
  
      read_page_data(fp->current_page, (uint8_t*)fp->buffer);
      return (true);
    }

    read_page_header(fp->current_page, &fp->page_header); // reload current header
  }
  debug_printf (("advance_file failed!\n\r"));
  return (false);
}

//-------------------------------------------------------------------------------------
static bool  fs_check_ram (uint16_t page)
{
  struct file_list *list_item = first_file_list_item;

 // debug_printf (("fs_check_ram %u\n\r", page));

  while ( list_item )
  { //First check open files
    if (list_item->fp->current_page == page)
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
static uint16_t fs_find_open_page(void)
{
  bool          over_wear = false;
  uint16_t      end_search_page;
  sector_info_t sector_info;
  page_info_t   pagei;
  uint16_t      free_pages_per_sector = 0xFFFF;

//  debug_printf (("fs_find_open_page\n\r"));
  
  if ( start_search_page < flash_first_page || start_search_page > flash_last_page )
    start_search_page = flash_first_page;
  
  end_search_page = start_search_page++;

 // debug_printf(("fs_find_open_page start @ %u\n\r",start_search_page));

  sector_info.bytes.flags = 0; // force sector info read

  do
  {
    for ( ; start_search_page <= flash_last_page; start_search_page++ )
    {
      if ((start_search_page % flash_pages_per_sector == 0) || (sector_info.bytes.flags != ERASE_COUNT_KEY))
        Read_Sector_Info( sector_number(start_search_page), &sector_info );

      if ( sector_info.bytes.flags != ERASE_COUNT_KEY )
      {
        debug_printf(("Sector check failed @ %d (%d)\n",sector_number(start_search_page), start_search_page));
        Flash_Erase_Sector(sector_number(start_search_page));   // wipe sector
        sector_info.erase_count = 0;                                  // set_erase_count first
        sector_info.bytes.flags = ERASE_COUNT_KEY;
        Write_Sector_Info(sector_number(start_search_page), &sector_info); // rewrite erase count
        return (start_search_page);
      }
      
      if ( (sector_info.erase_count & ERASE_COUNT_MASK) < wear_threshold )
      {
        read_page_header (start_search_page, &pagei);

        if ( pagei.file_num == 0xFFFF ) // cleared and ready to be used
        {
          if (fs_check_ram (start_search_page)) // not in use by open file
          {
            // debug_printf(("using - %u\n\r",start_search_page));
            return (start_search_page);
          }
          
          free_pages_per_sector = 0xFFFF; // cant erase this sector
        }
        else
        if ( pagei.file_num == 0 ) // ready to be erased - check if whole sector can be erased
        {
          if ( start_search_page % flash_pages_per_sector == 0 ) // first page per sector
          {
            free_pages_per_sector = 1;
          }
          else
          {
            if ( free_pages_per_sector < flash_pages_per_sector )
              free_pages_per_sector++;
          }
          
          if ( free_pages_per_sector == flash_pages_per_sector )
          {
            debug_printf(("garbage collected @ %d (%d - %d)!\n",sector_number(start_search_page), start_search_page - 7, start_search_page));
            Flash_Erase_Sector(sector_number(start_search_page)); // wipe sector
            sector_info.erase_count++;                                    // inc erase count
            sector_info.bytes.flags = ERASE_COUNT_KEY;
            Write_Sector_Info(sector_number(start_search_page), &sector_info); // rewrite erase count
            return (start_search_page); // this is the last page in this sector but so be it
          }
        }
        else // still in use
        {
          free_pages_per_sector = 0xFFFF; // cant erase this sector
        }
      }
      else
      {
        over_wear = true;
      }

//      else if ( pagei.file_num == 0xFFFF && !dead_page )
//      {
//        dead_page = start_search_page;
//      }
    }

    sector_info.bytes.flags = 0;
    free_pages_per_sector = 0xFFFF;
    
    for ( start_search_page = flash_first_page; start_search_page <= end_search_page ; start_search_page++ )
    {
      if ((start_search_page % flash_pages_per_sector == 0) || (sector_info.bytes.flags != ERASE_COUNT_KEY))
        Read_Sector_Info( start_search_page/flash_pages_per_sector, &sector_info );
      
      if ( sector_info.bytes.flags != ERASE_COUNT_KEY )
      {
        debug_printf(("Sector check failed @ %d (%d)\n",sector_number(start_search_page), start_search_page));
        Flash_Erase_Sector(sector_number(start_search_page)); // wipe sector
        sector_info.erase_count = 0;                                  // set_erase_count first
        sector_info.bytes.flags = ERASE_COUNT_KEY;
        Write_Sector_Info(sector_number(start_search_page), &sector_info); // rewrite erase count
        return (start_search_page);
      }
      
      if ( (sector_info.erase_count & ERASE_COUNT_MASK) < wear_threshold )
      {
        read_page_header (start_search_page, &pagei);

        if ( pagei.file_num == 0xFFFF ) // cleared and ready to be used
        {
          if (fs_check_ram (start_search_page)) // not in use by open file
          {
            // debug_printf(("using - %u\n\r",start_search_page));
            return (start_search_page);
          }
          
          free_pages_per_sector = 0xFFFF; // cant erase this sector
        }
        else
        if ( pagei.file_num == 0 ) // ready to be erased - check if whole sector can be erased
        {
          if ( start_search_page % flash_pages_per_sector == 0 ) // first page per sector
          {
            free_pages_per_sector = 1;
          }
          else
          {
            if ( free_pages_per_sector < flash_pages_per_sector )
              free_pages_per_sector++;
          }

          if ( free_pages_per_sector == flash_pages_per_sector )
          {
            debug_printf(("garbage collected @ %d (%d - %d)!\n",sector_number(start_search_page), start_search_page - 7, start_search_page));
            Flash_Erase_Sector(sector_number(start_search_page)); // wipe sector
            sector_info.erase_count++;                                    // inc erase count
            sector_info.bytes.flags = ERASE_COUNT_KEY;
            Write_Sector_Info(sector_number(start_search_page), &sector_info); // rewrite erase count
            return (start_search_page); // this is the last page in this sector but so be it
          }
        }
        else // still in use
        {
          free_pages_per_sector = 0xFFFF; // cant erase this sector - set high to fail test
        }
      }
      else
      {
        over_wear = true;
      }
    }
    
    if ( over_wear && (wear_threshold < MAX_WEAR_THRESHOLD) )
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
      return(0); // disk full
    }
  } while( 1 );
}

//-------------------------------------------------------------------------------------
static bool fs_write_page (uint16_t page, const page_info_t *header, const uint8_t *dat, char *filename)
{
 // debug_printf(("fs_write_page %u\n\r",page));
  debug_printf_page_header (page, header, filename);

  if ( !page || !header || !dat )
    return(false);

  if ( page < flash_first_page || page > flash_last_page )
  {
    debug_printf (("invalid page\n\r"));
    return(false);
  }

  flash_driver_set_flash_write(sector_number(page), page_header_offset(page));
  flash_driver_write((uint8_t *)header, page_info_size);
  flash_driver_write_release();
  
  flash_driver_set_flash_write(sector_number(page), page_data_offset(page));
  flash_driver_write(dat, page_data_size);
  flash_driver_write_release();
  
  // compare header to flash
  // compare data to flash 
    
  return(true);

//  fs_erase_page(page);
//  debug_printf(("fs_write_page failed\n\r"));
  
//  return(false);
}

//-------------------------------------------------------------------------------------
//find an available file block by checking the file number entry
static FS_FILE *fs_get_file_mem (void)
{
  struct file_list *list_item;

  pseffs_error = PSEFFS_ERROR_OUT_OF_MEM;
  
  if ( !first_file_list_item )
  {
    first_file_list_item = calloc( 1, sizeof(struct file_list) );
  }
  
  list_item = first_file_list_item;
  
  while ( list_item )
  {
    if ( list_item->fp == NULL )
    {
      list_item->fp = calloc(1,sizeof(FS_FILE));
      return list_item->fp; // could be null if out of mem
    }
    else
    {
      if ( list_item->next == NULL )
        list_item->next = calloc( 1, sizeof(struct file_list) );
    }
    
    list_item = list_item->next;
  }
  
  return NULL;
}

//-------------------------------------------------------------------------------------
static void fs_free_file_mem (FS_FILE *fp)
{
  struct file_list *list_item = first_file_list_item; 
  struct file_list *remove_item = NULL; 

  if ( fp )
  {
    if ( fp->buffer )
    {
      free(fp->buffer);
      fp->buffer = NULL;
    }
    
    while ( list_item )
    {
      if ( list_item->fp == fp )
      {
        free(fp);
        list_item->fp = NULL;
        remove_item = list_item;
        break;
      }
      
      list_item = list_item->next;
    }

    if ( first_file_list_item == remove_item )
    {
      first_file_list_item = first_file_list_item->next;
      free(remove_item);
      return;
    }
    
    list_item = first_file_list_item;
    
    while ( list_item )
    {
      if ( list_item->next == remove_item )
      {
        list_item->next = list_item->next->next;
        free(remove_item);
        return;
      }
      
      list_item = list_item->next;
    }

    // why are we still here
    
    debug_printf (("free_file_mem not found in linked list\n\r"));
  }
  else
  {
    debug_printf (("free_file_mem has no file pointer\n\r"));
  }
}

//-------------------------------------------------------------------------------------
static bool fp_valid(FS_FILE *fp)
{
  struct file_list *list_item = first_file_list_item;
  
  while ( list_item )
  {
    if ( list_item->fp == fp )
      return true;
    
    list_item = list_item->next;
  }
  
  return false;
}

//-------------------------------------------------------------------------------------
static bool  fs_format_fname (char *fname_buf, const char *fname)
{
  int i;
  
  if ( !fname_buf || !fname ) return false;
  
 // debug_printf (("fs_format_fname %s ", fname));
  for (i = 0; i < MAX_FNAME_SIZE; i++)
  {
    if ( fname[i] == '\0' )
    {
      break;
    }

    if ( fs_isprint( (int)fname[i] ) )
    {
      fname_buf[i] = fs_toupper( fname[i] );
    }
    else
    {
//      fname_buf[i] = '\0';
      debug_printf (("Ileagle char found -> 0x%02hhX @ %d !!!\n\r", fname[i], i));
      pseffs_error = PSEFFS_ERROR_ILEGALE_CHAR;
      fname_buf[i] = '\0';
      return(false);
    }
  }

  if (i < MAX_FNAME_SIZE) {fname_buf[i] = '\0';}
  else                    {fname_buf[MAX_FNAME_SIZE-1] = '\0';}

 // debug_printf (("-> %s\n\r", fname_buf));

  return(true);
}

//-------------------------------------------------------------------------------------
// Must pass uppercase filename to this function
static bool fs_file_is_open (char *fname)
{
  struct file_list *list_item = first_file_list_item;

  while ( list_item )
  {
    if ( list_item->fp )
    {
      if ( strcmp( list_item->fp->file_header.name, fname) == 0 )
      {
        pseffs_error = PSEFFS_ERROR_FILE_OPEN;
      //  debug_printf (("File %s is open\n\r", fname));
        return(true);
      }
    }
    
    list_item = list_item->next;
  }

  //debug_printf (("File %s not open\n\r", fname));
  return(false);
}

//-------------------------------------------------------------------------------------
// Must pass uppercase filename to this function
static bool  fs_find_file_info (char *uppercase_fname, FS_FILE *fp)
{
  //debug_printf (("fs_find_file_info for %s\n\r", uppercase_fname));
  
  if ( !fp ) return false;

  strcpy (fp->file_header.name, uppercase_fname); // ????? why do you do this Hein ?????
  fs_reset_fat();

  while( fs_read_fat_data(&current_fat, (uint8_t *)&fp->file_header, sizeof(file_info_t)) )
  {
    if ( strcmp( uppercase_fname, fp->file_header.name ) == 0 )
    {
      if ( fp->buffer == NULL ) 
        fp->buffer = malloc(page_data_size);
      
      if ( fp->buffer )
      {
        memset (fp->buffer, 0xFF, page_data_size);
        return(true);
      }
    }
  }

//  debug_printf (("fs_find_file_info Not found %s\n\r",uppercase_fname));
  return(false);
}

//-------------------------------------------------------------------------------------
// Must pass uppercase filename to this function
static bool fs_find_file_num (uint16_t filenum, FS_FILE *fp)
{
 // debug_printf (("fs_find_file_num %u\n\r", filenum));

  fs_reset_fat();

  while( fs_read_fat_data(&current_fat, (uint8_t *)&fp->file_header, sizeof(file_info_t)) )
  {
    if ( fp->file_header.file_num == filenum )
    {
     // debug_printf_file_header (&fp->file_header);
      return(true);
    }
  }

 // debug_printf (("fs_find_file_num Not found %d\n\r",filenum));
  return(false);
}

//-------------------------------------------------------------------------------------
static void fs_load_file (FS_FILE *fp)
{
 // debug_printf (("load_file %s\n\r", fp->file_header.name));
  fp->current_page = fp->file_header.start_page;  // reset file to start page & offset
  fp->current_offset = fp->file_header.start_offset;
  fp->position = 0;

  read_page (fp->current_page, &fp->page_header, fp->buffer);
}

//-------------------------------------------------------------------------------------
static bool  fs_advance_header(FS_FILE *fp)
{
  uint16_t next_link, next_page;

 // debug_printf (("fs_advance_header of file %s\n\r", fp->file_header.name));
  if (fp->page_header.next_page && fp->page_header.next_page != 0xFFFF)
  {
    next_link = next_link_num(fp->page_header.link_num);
    next_page = fp->page_header.next_page;

    read_page_header(next_page, &fp->page_header);

    if ((fp->page_header.file_num == fp->file_header.file_num) && (fp->page_header.link_num == next_link))
    {
      fp->current_page = next_page;       // set to next fat mem page
      return (true);
    }

    read_page_header(fp->current_page, &fp->page_header);
  }

  debug_printf (("fs_advance_header of file %s failed!\n\r", fp->file_header.name));
  return (false);
}

//-------------------------------------------------------------------------------------
static bool  fs_reverse_header(FS_FILE *fp)
{
  uint16_t prev_link, prev_page;

  //debug_printf (("fs_reverse_header of file %s\n\r", fp->file_header.name));
  if (fp->page_header.prev_page)
  {
    prev_link = previous_link_num(fp->page_header.link_num);
    prev_page = fp->page_header.prev_page;

    read_page_header(prev_page, &fp->page_header);

    if ((fp->page_header.file_num == fp->file_header.file_num) && (fp->page_header.link_num == prev_link))
    {
      fp->current_page = prev_page;       // set to next fat mem page
      return (true);
    }

    read_page_header(fp->current_page, &fp->page_header);
  }

  debug_printf (("fs_reverse_header of file %s failed!\n\r", fp->file_header.name));
  return (false);
}

//-------------------------------------------------------------------------------------
static bool  fs_get_file_num (FS_FILE *fp)
{
  struct file_list *list_item = first_file_list_item;
  file_info_t spare_header;
  uint16_t first_file_num = next_file_num;

  //debug_printf (("fs_get_file_num\n\r"));
  
  if (!fp) return false;

  do
  {
    fs_reset_fat();
  
    while( fs_read_fat_data(&current_fat, (uint8_t *)&spare_header, sizeof(spare_header)) )
    {
      if ( spare_header.file_num == next_file_num )
      {
        fs_reset_fat(); // restart search with new file num
        next_file_num++;

        if ( next_file_num >= FIRST_FAT_FILE_NUM )
        {
          next_file_num = 1;
        }
        
        if ( next_file_num == first_file_num ) // searched all posible file numbers
          return false;
      }
    }

    while ( list_item )
    {
      if ( list_item->fp )
      {
        if (list_item->fp->file_header.file_num == next_file_num)
        {
          list_item = first_file_list_item; // restart search with new file num
          next_file_num++;

          if ( next_file_num >= FIRST_FAT_FILE_NUM )
          {
            next_file_num = 1;
          }
          
          if ( next_file_num == first_file_num ) // searched all posible file numbers
            return false;
          
          break; // from while ( list_item )
        }
      }
      
      list_item = list_item->next;
    }
    
    if ( !list_item ) // nothing found in ram
    {
      fp->file_header.file_num = next_file_num;
      return true;
    }
    
  } while ( 1 );
}

//-------------------------------------------------------------------------------------
static bool  fs_init_new_file (char *uppercase_fname, FS_FILE *fp)
{
 // debug_printf (("init_new_file %s\n\r", uppercase_fname));

  if (fp)
  {
    if ( fs_get_file_num(fp) )
    {
      strcpy (fp->file_header.name, uppercase_fname);
  
      fp->file_header.start_page = fs_find_open_page();
     // debug_printf (("start_page %u\n\r", fp->file_header.start_page));
  
      if ( !fp->file_header.start_page ) return(false);
  
      fp->file_header.start_offset = 0;
      fp->file_header.file_size = 0;
  
      fp->page_header.file_num = fp->file_header.file_num;
      fp->page_header.prev_page = 0;
      fp->page_header.next_page = 0xFFFF;
      fp->page_header.link_num = 0;
    
      fp->current_page = fp->file_header.start_page;
      fp->current_offset = 0;
      fp->position = 0;
      
      if ( fp->buffer == NULL ) 
        fp->buffer = malloc(page_data_size);
      
      if ( fp->buffer )
      {
        memset (fp->buffer, 0xFF, page_data_size);
        return(true);
      }
    }
  }

  return(false);
}

//-------------------------------------------------------------------------------------
#ifdef RESET_FILE
static void fs_reset_file (FS_FILE *fp)
{
 // debug_printf (("reset_file %s\n\r", fp->file_header.name));
  if (fp->current_page != fp->file_header.start_page)
  {
    fs_load_file (fp);
  }
  else
  {
    fp->current_offset = fp->file_header.start_offset;
    fp->position = 0;      
  }
}
#endif

//-------------------------------------------------------------------------------------
void reset_file_list(void)
{
  if ( fs_mutex_take() )
  {
  //debug_printf (("reset_file_list\n\r"));
    fs_reset_fat();
    fs_mutex_give();
  }
}

/*-----------------------------------------------------------------------------------*/
int32_t read_file_list(char *name, uint16_t *filenum, uint32_t *filesize)
{
  if ( fs_mutex_take() )
  {
    file_info_t spare_header;
   // debug_printf (("read_file_list "));
    while( fs_read_fat_data(&current_fat, (uint8_t *)&spare_header, sizeof(spare_header)) )
    {
      if ( strlen( spare_header.name ) )
      {
        strcpy(name,spare_header.name);
    //    debug_printf (("-> %s\n\r", name));
        if ( filenum )  *filenum  = spare_header.file_num;
        if ( filesize ) *filesize = spare_header.file_size;
        fs_mutex_give();
        return ((int32_t)spare_header.file_size);
      }
    }
    debug_printf (("NOT_FOUND\n\r"));
    
    fs_mutex_give();
  }
  
  return (-1);
}

//-------------------------------------------------------------------------------------
void Format_fs(void)
{
  page_info_t page_header;
  unsigned int page_number;

  if ( !flash_last_page && !init_flash() )
  { // big problem
    fs_error_callback("NO FLASH MEDIA AVAILABLE");
    return;
  }
  
  fs_fcloseall();
  
  if ( fs_mutex_take() )
  {
    //debug_printf (("Format_fs\n"));
    for ( page_number = flash_first_page ; page_number <= flash_last_page ; page_number++ )
    {
      Format_Progress_Callback(page_number, flash_last_page);

      read_page_header(page_number, &page_header);

      if ( page_header.file_num != 0 && page_header.file_num != 0xFFFF )
      {
        fs_erase_page (page_number);
      }
      
      watchdog_reset_callback();
    }
    
    memset(&current_fat,0,sizeof(FAT));
    fs_mutex_give();
  }
}

//-------------------------------------------------------------------------------------
// Small FAT Functions
//-------------------------------------------------------------------------------------
static bool  fs_init_fat(FAT *fat)
{
  if ( fat )
  {
    if ( fs_find_fat(fat) )
      return true;
      
    if ( fs_create_fat(fat, FIRST_FAT_FILE_NUM) )
      return true;
  }
  
  debug_printf (("fs_init_fat failed!\n\r"));
  return(false);
}

//-------------------------------------------------------------------------------------
static void fs_reset_fat(void)
{
  //debug_printf (("fs_reset_fat - "));

  if ( current_fat.start_page )
  {
    current_fat.current_offset = 0;        // fat must always start on the first char of the page

    if (current_fat.current_page != current_fat.start_page)
    {
      current_fat.current_page = current_fat.start_page;  // reset fat to start

      if ( !current_fat.buffer )
        current_fat.buffer = malloc(page_data_size);
    
      if ( current_fat.buffer )
        read_page ( current_fat.current_page, &current_fat.page_header, current_fat.buffer );
      
     // debug_printf (("reloaded header/buffer\n\r"));
    }

   // debug_printf (("start page: %u\n\r", fat.start_page));
  }
  else
  {
    debug_printf (("fs_reset_fat - failed - no start page\n\r"));
    fs_init_fat(&current_fat);
  }
}

//-------------------------------------------------------------------------------------
static bool  fs_find_fat( FAT *fat )
{
  page_info_t page_header;
  uint16_t page;                       // page index - hopefully the memory device has less than 64k pages else mak this 32 bit
  uint16_t saved_fat_page;
  uint16_t better_fat_file_num;

  if ( !fat )
    return false;
    
  if ( !flash_last_page && !init_flash() )
  { // big problem
    fs_error_callback("NO FLASH MEDIA AVAILABLE");
    return false;
  }

  do
  {
    better_fat_file_num = 0;
    
 // debug_printf (("fs_find_fat\n\r"));
    for ( page = flash_first_page ; page <= flash_last_page ; page++ )
    {
      read_page_header (page, &page_header);
      
      if ( better_fat_file_num )
      {
        if ( page_header.file_num == better_fat_file_num )
        {
          saved_fat_page = page;
          better_fat_file_num++;
          if ( better_fat_file_num == 0xFFFF )
            better_fat_file_num = FIRST_FAT_FILE_NUM;
        }
      }
      else
      {
        if ( page_header.file_num >= FIRST_FAT_FILE_NUM && page_header.file_num != 0xFFFF )   // found a fat page
        {
          saved_fat_page = page;
          better_fat_file_num = page_header.file_num + 1;
          if ( better_fat_file_num == 0xFFFF )
            better_fat_file_num = FIRST_FAT_FILE_NUM;
        }
      }
    }
  
    if (better_fat_file_num)
    {
    //  debug_printf(("found fat page @ %u\n\r",page));
      fat->start_page = fs_check_file_chain( saved_fat_page );
      if ( fat->start_page )
      {
        fat->current_page = fat->start_page;
        fat->current_offset = 0;
        if ( !fat->buffer ) fat->buffer = malloc(page_data_size);
        if ( fat->buffer )
        {
          read_page ( fat->current_page, &fat->page_header, fat->buffer );
      //    debug_printf(("fat start page = %u\n\r",fat.start_page));
          return(true);
        }
        
        pseffs_error = PSEFFS_ERROR_OUT_OF_MEM;
      }
      else // fat file chain broken - wipe it look for a previous version.
      {    // we may have died while writing the fat so file fragments may remain - scan will clear it out
        fs_remove_file_chain(saved_fat_page);
        if ( fat->buffer ) free(fat->buffer);
        memset(&fat,0,sizeof(fat));
      }
    }
  } while(better_fat_file_num);

  debug_printf (("fs_find_fat failed!\n\r"));
  return(false);
}

//-------------------------------------------------------------------------------------
static bool  fs_create_fat( FAT *fat, uint16_t fat_file_num )
{
  if ( !fat )
    return false;

  if ( !flash_last_page && !init_flash() )
  { // big problem
    fs_error_callback("NO FLASH MEDIA AVAILABLE");
    return false;
  }
  
  //debug_printf (("fs_create_fat\n\r"));
  fat->page_header.file_num = fat_file_num;
  fat->page_header.prev_page = 0;
  fat->page_header.next_page = 0xFFFF;
  fat->page_header.link_num = 0;

  fat->start_page = fs_find_open_page();

  if ( fat->start_page )
  {
    fat->current_page = fat->start_page;
    fat->current_offset = 0;
    
    fat->buffer = malloc(page_data_size);
    
    if ( fat->buffer )
    {
      memset ( fat->buffer, 0xFF, page_data_size );
      return (true);
    }
    
    pseffs_error = PSEFFS_ERROR_OUT_OF_MEM;
  }
  else
  {
    pseffs_error = PSEFFS_ERROR_MEDIA_FULL;
  }
  
  debug_printf (("fs_create_fat failed!\n\r"));
  return(false);
}

//-------------------------------------------------------------------------------------
static bool fs_advance_fat( FAT *fat )
{
  uint16_t next_link, next_page, fat_file_num;

  if ( !fat )
    return false;

 // debug_printf (("fs_advance_fat\n\r"));
  if (fat->page_header.next_page && fat->page_header.next_page != 0xFFFF)
  {
    fat_file_num  = fat->page_header.file_num;
    next_link     = next_link_num(fat->page_header.link_num);
    next_page     = fat->page_header.next_page;

    read_page_header(next_page, &fat->page_header);

    if ((fat->page_header.file_num == fat_file_num) && (fat->page_header.link_num == next_link))
    {
      fat->current_page = next_page;      // set to next fat mem page
      fat->current_offset = 0;            // reset file index

      if ( fat->buffer )
      {
        read_page_data ( fat->current_page, fat->buffer );
        return (true);
      }
    }
    
    read_page_header ( fat->current_page, &fat->page_header );
  }
  
  debug_printf (("fs_advance_fat failed! %hX %hu %hu %hu\n\r", fat->page_header.file_num
                                                              ,fat->page_header.next_page
                                                              ,fat->page_header.prev_page
                                                              ,fat->page_header.link_num   ));
  return (false);
}

//-------------------------------------------------------------------------------------
static bool  fs_read_fat_data (FAT *fat, uint8_t *data, uint16_t len)
{
  int i;

  if ( !data || !len || !fat || !fat->buffer )
    return false;

  if ( fat->buffer[fat->current_offset] == FAT_EOF )
    return false;

//  debug_printf (("fs_read_fat_entry\n\r"));

  for (i = 0; i < len; i++)
  {
    data[i] = fat->buffer[fat->current_offset++];

    if (fat->current_offset == page_data_size) // test for end of page (in ram)
    {
      if (!fs_advance_fat(fat))
      {
        debug_printf (("fs_read_fat_entry failed!\n\r"));
        return (false);
      }
    }
  }

  return(true);
}

//-------------------------------------------------------------------------------------
static bool  fs_write_fat_data (FAT *fat, uint8_t *data, uint16_t len, char *fat_name)
{
  int i;

  if ( !data || !len || !fat || !fat->buffer )
    return false;
  
  for ( i = 0 ; i < len; i++ )
  {
    fat->buffer[fat->current_offset++] = data[i]; // step source pointer

    if (fat->current_offset == page_data_size) // test for end of ram buffer and write changed data
    {
      fat->page_header.next_page = fs_find_open_page();

      if ( fat->page_header.next_page )
      {
        if ( fs_write_page(fat->current_page, &fat->page_header, fat->buffer, fat_name) )
        {
          fat->page_header.prev_page = fat->current_page;
          fat->current_page = fat->page_header.next_page;
          fat->page_header.next_page = 0xFFFF;
          fat->page_header.link_num = next_link_num(fat->page_header.link_num);
          fat->current_offset = 0;
          memset( fat->buffer,FAT_EOF, page_data_size ); // prob not needed
       //   debug_printf(("Fat start page = %u, Fat Current page = %u\n\r",fat.start_page,fat.current_page));
       //   debug_printf_page_header (&fat.page_header);
        }
        else // fat write current page failed
        {
          if ( !pseffs_error )
          {
            pseffs_error = PSEFFS_ERROR_MEDIA_ERROR;
          }
          debug_printf (("fs_write_fat_data failed with error - flash write error\n\r"));
          debug_printf_pseffs_error (pseffs_error);
          return(false);
        }
      }
      else  // file sys full - error critical at this stage
      {
        if ( !pseffs_error )
        {
          pseffs_error = PSEFFS_ERROR_MEDIA_FULL;
        }
        debug_printf (("fs_write_fat_data failed with error - no space\n\r"));
        debug_printf_pseffs_error (pseffs_error);
        return(false);
      }
    }
  }
  
  fat->buffer[fat->current_offset] = FAT_EOF;
  
 // debug_printf (("fat current offset: %u\n\r", fat.current_offset));
  return(true);
}

//-------------------------------------------------------------------------------------
static int32_t fs_fat_file_search (char *fname)
{
  file_info_t file_header;

 // debug_printf (("fs_fat_file_search %s\n\r", fname));
  fs_reset_fat();

  while( fs_read_fat_data(&current_fat, (uint8_t *)&file_header, sizeof(file_header)) )
  {
    if (strcmp (file_header.name, fname) == 0)  // check if filename is the one we are looking for
    {
      //debug_printf_file_header (&file_header);
      return ((int32_t) file_header.file_size);
    }
  }
  //debug_printf (("not found\n\r"));
  return (-1);
}

//-------------------------------------------------------------------------------------
static bool  fs_update_fat (file_info_t *new_fat_entry)
{
  file_info_t   fat_entry;
  bool updated = false;
//  uint16_t current_fat_page = 0;
  uint16_t new_fat_file_num = current_fat.page_header.file_num + 1;
  
  FAT new_fat;
  
  if ( new_fat_file_num == 0xFFFF )
    new_fat_file_num = FIRST_FAT_FILE_NUM;

 // debug_printf (("fs_update_fat "));
 // debug_printf_file_header (file_header);
  
  RETRY_UPDATE_FAT:
  
  fs_reset_fat();                                                       // reset current fat
  
  if ( fs_create_fat(&new_fat, new_fat_file_num) )
  {
    while( fs_read_fat_data(&current_fat, (uint8_t *)&fat_entry, sizeof(fat_entry)) )
    {
      if ( strcmp (new_fat_entry->name, fat_entry.name) == 0 ) // same file name
      {
        if ( new_fat_entry->file_num != 0 ) // update file details
        {
          if (fs_write_fat_data (&new_fat, (uint8_t *)new_fat_entry, sizeof(file_info_t), "NEW FAT"))
          {
            updated = true;
          }
          else
          {
            debug_printf (("fs_update_fat failed writing new file header!\n\r"));
            fs_remove_file_fragments(new_fat_file_num);
            free(new_fat.buffer);
            goto RETRY_UPDATE_FAT;
          }
        }
      }
      else
      if ( fat_entry.file_num != 0 && new_fat_entry->file_num == fat_entry.file_num )
      { // Update file data (modify)
        if (fs_write_fat_data (&new_fat, (uint8_t *)new_fat_entry, sizeof(file_info_t), "NEW FAT"))
        {
          updated = true;
        }
        else
        {
          debug_printf (("fs_update_fat failed writing new file header!\n\r"));
          fs_remove_file_fragments(new_fat_file_num);
          free(new_fat.buffer);
          goto RETRY_UPDATE_FAT;
        }
      }
      else
      if (fat_entry.file_num != 0)
      { //Copy entry across
        if ( !fs_write_fat_data (&new_fat, (uint8_t *)&fat_entry, sizeof(fat_entry), "NEW FAT"))
        {
          debug_printf (("fs_update_fat failed writing existing file header!\n\r"));
          fs_remove_file_fragments(new_fat_file_num);
          free(new_fat.buffer);
          goto RETRY_UPDATE_FAT;
        }
      }
    }

    if ( !updated && new_fat_entry->file_num ) // new file
    {
      if ( !fs_write_fat_data (&new_fat, (uint8_t *)new_fat_entry, sizeof(file_info_t), "NEW FAT"))
      {
        debug_printf (("fs_update_fat failed update!\n\r"));
        fs_remove_file_fragments(new_fat_file_num);
        free(new_fat.buffer);
        goto RETRY_UPDATE_FAT;
      }
    }

    if ( !fs_write_page(new_fat.current_page, &new_fat.page_header, new_fat.buffer, "NEW FAT") )
    {
      debug_printf (("fs_update_fat failed on fs_write_page!\n\r"));
      fs_remove_file_fragments(new_fat_file_num);
      free(new_fat.buffer);
      goto RETRY_UPDATE_FAT;
    }
    
    // seems all went well - remove old fat
    fs_remove_file_chain(current_fat.start_page);
    free(current_fat.buffer);
    
    memcpy( &current_fat, &new_fat, sizeof(current_fat) );

    //debug_printf (("fs_update_fat updated\n\r"));
    return(true);
  }

  debug_printf (("fs_update_fat failed!\n\r"));
  return(false);
}

//-------------------------------------------------------------------------------------
static bool  fs_fat_remove (char *file_name)
{
  file_info_t fat_entry;

  fs_reset_fat();

  while( fs_read_fat_data(&current_fat, (uint8_t *)&fat_entry, sizeof(fat_entry)) )
  {
    if ( strcmp( file_name, fat_entry.name ) == 0 )
    {
      fat_entry.file_num = 0;
      
      if ( fs_update_fat(&fat_entry ) )
      {
        return(true);
      }
    }
  }

  debug_printf (("fs_fat_remove failed!\n\r"));
  return(false);
}

/*-----------------------------------------------------------------------------------*/
static bool fs_fat_remove_num (uint16_t file_num)
{
  file_info_t   fat_entry;

 // debug_printf (("fs_fat_remove_num %u\n\r",file_num));

  fs_reset_fat();

  while( fs_read_fat_data(&current_fat, (uint8_t *)&fat_entry, sizeof(fat_entry)) )
  {
    if ( file_num == fat_entry.file_num )
    {
      fat_entry.file_num = 0;
      
      if ( fs_update_fat(&fat_entry ) )
      {
        return(true);
      }
    }
  }
  
  return(false);
}

//-------------------------------------------------------------------------------------
static void debug_printf_pseffs_error (uint8_t error)
{
  switch (error)
  {
    case PSEFFS_ERROR_ILEGALE_CHAR:
      debug_printf (("PSEFFS_ERROR_ILEGALE_CHAR\n\r"));
      break;
    case PSEFFS_ERROR_FILE_NAME_IN_USE:
      debug_printf (("PSEFFS_ERROR_FILE_NAME_IN_USE\n\r"));
      break;
    case PSEFFS_ERROR_FILE_NOT_FOUND:
      debug_printf (("PSEFFS_ERROR_FILE_NOT_FOUND\n\r"));
      break;
    case PSEFFS_ERROR_MEDIA_FULL:
      debug_printf (("PSEFFS_ERROR_MEDIA_FULL\n\r"));
      break;
    case PSEFFS_ERROR_MEDIA_ERROR:
      debug_printf (("PSEFFS_ERROR_MEDIA_ERROR\n\r"));
      break;
    case PSEFFS_ERROR_FILE_OPEN:
      debug_printf (("PSEFFS_ERROR_FILE_OPEN\n\r"));
      break;
    case PSEFFS_ERROR_OUT_OF_MEM:
      debug_printf (("PSEFFS_ERROR_OUT_OF_MEM\n\r"));
      break;
    case PSEFFS_ERROR_INVALID_SWITCH:
      debug_printf (("PSEFFS_ERROR_INVALID_SWITCH\n\r"));
      break;
    case PSEFFS_ERROR_EOF:
      debug_printf (("PSEFFS_ERROR_EOF\n\r"));
      break;
    case PSEFFS_ERROR_NO_PERMISSION:
      debug_printf (("PSEFFS_ERROR_NO_PERMISSION\n\r"));
      break;
    default:
      debug_printf (("Unknown error number %u\n\r", pseffs_error));
      break;
  }
}

/*-----------------------------------------------------------------------------------*/
void fs_scan_media(void)
{
  uint16_t page;//, dead_pages = 0;
  page_info_t page_header;
  file_info_t fat_entry;
  bool linked = false;
  char uppercase_fname[MAX_FNAME_SIZE];

  //printf("Scanning media from %u to %u\n\r",FLASH_FILE.file_system_start, FLASH_FILE.file_system_end);
  scan_start_callback(flash_first_page,flash_last_page);

  fs_reset_fat();
  
  while( fs_read_fat_data(&current_fat, (uint8_t *)&fat_entry, sizeof(fat_entry)) )
  {
    watchdog_reset_callback();
    
    if ( fat_entry.file_num && fat_entry.file_num != 0xFFFF )
    {
      if ( !fs_format_fname(uppercase_fname,fat_entry.name) || !fs_check_file_chain(fat_entry.start_page) )
      {
        //printf("Integrity check for file %u %s failed - removing\n\r", file_header.file_num, file_header.name );
        scan_integrity_fail_callback(fat_entry.file_num,uppercase_fname);
        fs_remove_file_chain(fat_entry.start_page);
        fs_remove_file_fragments(fat_entry.file_num);

        //(void)fs_fat_remove(file_header.name);
        if ( !fs_remove_num(fat_entry.file_num) ) break;
        
        fs_reset_fat();
      }
    }
  }
  
  for ( page = flash_first_page; page <= flash_last_page; page++ )
  {
    watchdog_reset_callback();
    
    linked = false;

    read_page_header ( page, &page_header ); // get page header

    if ( page_header.file_num && page_header.file_num != 0xFFFF )
    {
      if ( page_header.file_num >= 1 && page_header.file_num < FIRST_FAT_FILE_NUM )
      {
//        if ( page_header.file_num == 0xFFFF ) // dead_page
//        {
//          dead_pages++;
//          //printf("Dead page - %u\n\r",page);
//          scan_dead_page_callback(page);
//        }
//        else
//        {
        fs_reset_fat();
        while( !linked && fs_read_fat_data(&current_fat, (uint8_t *)&fat_entry, sizeof(fat_entry)) )
        {
          watchdog_reset_callback();
              
          if ( fat_entry.file_num == page_header.file_num )
          {
            if ( fat_entry.start_page == page )
            {
              linked = true;
                  //printf ( "File %u %s, Page @ %u - link number %u\n\r", file_header.file_num, file_header.name, page, page_header.link_num );
              scan_file_info_callback(fat_entry.file_num,fat_entry.name,page,page_header.link_num);
            }
            else
            {
              read_page_header( fat_entry.start_page, &page_header );

              while ( page_header.next_page && (page_header.file_num == fat_entry.file_num) )
              {
                watchdog_reset_callback();
                    
                if ( page_header.next_page == page )
                {
                  linked = true;
                  read_page_header ( page_header.next_page, &page_header );
                  //printf ( "File %u %s, Page @ %u - link number %u\n\r", file_header.file_num, file_header.name, page, page_header.link_num );
                  scan_file_info_callback(fat_entry.file_num,fat_entry.name,page,page_header.link_num);
                  break; // from while ( page_header.next_page )
                }
                else
                {
                  read_page_header ( page_header.next_page, &page_header ); // get prev header
                }
              }
            }
          }
        }
      }
      else // fat page
      if ( page_header.file_num >= FIRST_FAT_FILE_NUM ) // fat page
      {
        fs_reset_fat();

        do
        {
          watchdog_reset_callback();
          
          if ( current_fat.current_page == page )
          {
            linked = true;
            break;
          }
        } while ( fs_advance_fat(&current_fat) );
      }
      else // fatbak page
      {
        // ?????????
      }

      if ( !linked )
      {
//        if ( page_header.file_num == 0xFFFF )
//        {
//          //printf("Dead page found @ %u\n\r", page);
//          scan_dead_page_callback(page);
//          fs_erase_page(page);
//          //printf("Page erased\n");
//          scan_page_erased_callback(page);
//        }
//        else
        {
          //printf("Fragment found of File num %u, @ page %u\n\r",page_header.file_num, page);
					fs_printf("Fragment found of File num %u, @ page %u\n\r",page_header.file_num, page);
          scan_fragment_callback(page_header.file_num,page);
          fs_erase_page(page);
          //printf("Page erased\n");
          scan_page_erased_callback(page);
        }
      }
    }
  }

  //printf("scanning done - %u dead pages found\n\r", dead_pages);
//  scan_end_callback(dead_pages);
  scan_end_callback(0);
}

//-------------------------------------------------------------------------------------
//void fs_test(void)
//{
//   char fname[MAX_FNAME_SIZE+1];
//   char fname2[MAX_FNAME_SIZE+1];
//   char c;
//   int i, count;
//  
//   FS_FILE *fp;
//
//// test fs
//// write 30 files of 10K
//   for ( i = 0; i < 100; i ++ )
//   {
//     sprintf( fname, "FILE_%d", i );
//     debug_printf(("\tWriting %s\n\r",fname));
//
//     fp = fs_fopen( fname, 'w' );
//
//     if ( fp )
//     {
//       c = 0;
//       count = 0;
//
//       do
//       {
//         poll_callback();
//
//         if ( fs_isprint(c) )
//         {
//           if ( !fs_fputc(c,fp) )
//           {
//             debug_printf((0,3,'C',"fputc error @ %d\n\r",count));
//             while(1);
//           }
//           count++;
//         }
//
//         c++;
//       } while ( count < 5000 ) ;
//       fs_fclose(fp);
//     }
//   }
//
//// rewrite 30 files of 10K again
//   for ( i = 0; i < 100; i ++ )
//   {
//     sprintf( fname, "FILE_%d", i );
//     debug_printf(("\tRewriting %s\n\r",fname));
//
//     fp = fs_fopen( fname, 'w' );
//
//     if ( fp )
//     {
//       c = 0;
//       count = 0;
//
//       do
//       {
//         poll_callback();
//
//         if ( fs_isprint(c) )
//         {
//           if ( !fs_fputc(c,fp) )
//           {
//             debug_printf(("\tfputc error @ %d\n\r",count));
//             while(1);
//           }
//           count++;
//         }
//
//         c++;
//       } while ( count < 6000 ) ;
//       fs_fclose(fp);
//     }
//   }
//
//// rename 30 files
//   for ( i = 0; i < 100; i ++ )
//   {
//     poll_callback();
//
//     sprintf( fname, "FILE_%d", i );
//     sprintf( fname2, "FILE_%d", i+100 );
//
//     debug_printf(("\tRenaming %s to %s\n\r",fname,fname2));
//
//     fs_rename( fname, fname2 );
//   }
//
//// apend 30 files
//   for ( i = 100; i < 200; i ++ )
//   {
//     sprintf( fname, "FILE_%d", i );
//     debug_printf(("\tAppending %s\n\r",fname));
//
//     fp = fs_fopen( fname, 'a' );
//
//     if ( fp )
//     {
//       c = 0;
//       count = 0;
//
//       fs_fputs("Appending from here ->",fp);
//
//       do
//       {
//         poll_callback();
//
//         if ( fs_isprint(c) )
//         {
//           if ( !fs_fputc(c,fp) )
//           {
//             debug_printf(("\tfputc error @ %d\n\r",count));
//             while(1);
//           }
//           count++;
//         }
//
//         c++;
//       } while ( count < 2000 ) ;
//       fs_fclose(fp);
//     }
//   }
//
//// open MAX_OPEN_FILE files for apending
//
//// fclip 30 files
//   for ( i = 100; i < 200; i ++ )
//   {
//     poll_callback();
//
//     sprintf( fname, "FILE_%d", i );
//
//     debug_printf(("\tCliping 1000 from %s\n\r",fname));
//
//     fclip (fname, 1000);
//   }
//
//// remove 30 files
//   for ( i = 100; i < 200; i ++ )
//   {
//     poll_callback();
//
//     sprintf( fname, "FILE_%d", i );
//
//     debug_printf(("\tRemoving %s\n\r",fname));
//
//     fs_remove( fname );
//   }
//}

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
//  for ( i = 0; i < FLASH_FILE.file_system_end; i++ )
//  {
//    sprintf(temp_buf,"I am page %u", i );
//    Write_Flash_Data(i, 77, temp_buf, sizeof(temp_buf) );
//    DEBUG(("writing @ %u - %s\n\r",i,temp_buf));
//  }
//
//  for ( i = 0; i < FLASH_FILE.file_system_end; i++ )
//  {
//    Read_Flash(i, 77, temp_buf, sizeof(temp_buf) );
//
//    DEBUG(("reading %u - %s\n\r",i, temp_buf));
//  }
//}

//-------------------------------------------------------------------------------------
#ifdef FS_FILEINFO
void fs_fileinfo (uint16_t *num_files, uint32_t *used_size, uint16_t *fat_size, uint32_t *sys_size )
{
  file_info_t spare_header;

  *num_files = 0;
  *used_size = 0;
  *fat_size = 0;

  //debug_printf (("fs_fileinfo "));
  fs_reset_fat ();

  while ( fs_read_fat_entry (&spare_header) )
  {
    *fat_size += sizeof(file_info_t);

    if ( ( strlen(spare_header.name) ) && ( spare_header.file_num != 0 ) )
    {
      *num_files += 1;
      *used_size += spare_header.file_size;
    }
  }

  *sys_size = (uint32_t)( (FLASH_FILE.file_system_end - FLASH_FILE.file_system_start) + 1 ) * (uint32_t)page_data_size;
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
//  uint16_t page;

//  file_info_t file_header;
//  page_info_t page_header;

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
//    printf ("FAT start page: %u\n", fat.start_page);

//    do
//    {
//      read_page_header(page,(uint8_t *) &page_header);

//      printf("Page %u: Wearlevel %u, file_num %u, prev_page %u, next_page %u, link_num %u\n",
//                                                                    page, Read_Erase_Count(page), 
//                                                                      page_header.file_num, page_header.prev_page,
//                                                                        page_header.next_page, page_header.link_num );
//      while( (fat.current_page == page) && (fat.buffer[fat.current_offset] != FAT_EOF) )
//      {
//        if ( fs_read_fat_entry(&file_header) )
//        {
//          printf("File num %u; Name %s; Start %u; Start Offset %u; Size %u\n",
//                                                                        file_header.file_num,
//                                                                          file_header.name,
//                                                                            file_header.start_page,
//                                                                              file_header.start_offset,
//                                                                                file_header.file_size );
//        }
//      }

//      page = fat.current_page;
//    } while( fat.buffer[fat.current_offset] != FAT_EOF );
//  }
//  else
//  {
//    printf ("ERROR - FAT not initialized\n");
//  }
//}

//-------------------------------------------------------------------------------------
//void fs_print_sector(uint16_t sector)
//{
//  page_info_t   page_header;
//  uint16_t    start_page,end_page;
//  uint8_t     address[4], c;

//  //debug_printf (("print_fs_sector %u\n", sector));
//  get_flash_sector_boundry(sector,&start_page,&end_page);

//  do
//  {
//    int i;

//    read_page_header(start_page,(uint8_t *) &page_header);

//    printf("Page %u: Wearlevel %u, file_num %u, prev_page %u, next_page %u, link_num %u\n",
//      start_page, Read_Erase_Count(start_page), 
//        page_header.file_num, page_header.prev_page,
//          page_header.prev_page, page_header.link_num );

//    if ( page_header.file_num == 0 )
//    {
//      printf( "Page available\n" );
//    }
//    else if ( page_header.file_num > 0 && page_header.file_num <= 2 )
//    {
//      printf( "FAT page\n" );
//    }
//      
//    setupFlashAddress(address,start_page, FS_DATA_IDX);
//    (void)sendFlashCommand(READ_CONTINUOUS_COMMAND,address); // READ THE FLASH DIRECTLY

//    for ( i = 0; i < page_data_size; i++ )
//    {
////      printf("0x%02X",flash_inb());
//      c = spi_read_byte();
//      if (fs_isprint (c)) {printf ("%c", c);}
//      else             {printf ("(%02X)", (uint16_t)c);}
//    }

//    Deselect_Flash();

//    printf("\n\n\n");
//    start_page++;
//    //poll_callback();
//  } while ( start_page <= end_page );
//}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
