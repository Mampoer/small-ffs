//-------------------------------------------------------------------------------------
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "fs.h"

//-------------------------------------------------------------------------------------
void DEBUG_RAW_printf (const char *ptr, ...);
void DEBUG_printf (int level, char *file, int line, const char *ptr, ...);

//-------------------------------------------------------------------------------------
#define debug_printf(...) {DEBUG_printf (0, "FS TEST", __LINE__, __VA_ARGS__);}

//-------------------------------------------------------------------------------------
const char *print_pseffs_error (void);

//-------------------------------------------------------------------------------------
void test_clip (void)
{
  int total_size = 0;
  int size_track = 0;
  int grow_size;
  int clip_size;
  int ansi_char = ' ';
  
  remove ("fclip_test");
  
  do
  {
    FILE *fp = fopen ("fclip_test", "a");

    if (fp)
    {
      grow_size = (rand () % 999) + 1;
      
      while (grow_size)
      {
        if ((total_size % 0xFF0) == 0)
        {
          debug_printf ("fputc break @ %d\n\r", total_size);
        }
        
        if (fputc (ansi_char++, fp) == EOF)
        {
          debug_printf ("fputc error @ %d\n\r", size_track);
        }
        
        if (!isprint (ansi_char))
          ansi_char = ' ';
        
        size_track++;
        total_size++;
        grow_size--;
      }
      
      fclose (fp);
      
      clip_size = rand () % size_track;
      
      if (clip_size == size_track)
        clip_size /= 3;
      
      fs_fclip ("fclip_test", clip_size);
      
      size_track -= clip_size;
      
      if (size_track > 0)
      {
        if (fs_access ("fclip_test") != size_track)
        {
          debug_printf ("file_size error @ %d [%d], %d\n\r", size_track, fs_access ("fclip_test"), total_size);
          while (1);
        }
      }
      else
      {
        if (fs_access ("fclip_test") != -1)
        {
          debug_printf ("file_size error @ %d [%d], %d\n\r", size_track, fs_access ("fclip_test"), total_size);
          while (1);
        }
      }
      
      debug_printf ("Pass %d [%d], %d, %d\n\r", size_track, fs_access ("fclip_test"), clip_size, total_size);
    }
  } while (1);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void print_text_file(const char *fname)
{
  char buf[256];
  
  FILE *fp;
  
  fp = fopen (fname, "r");
  
  if (fp)
  {
    debug_printf("\n\rPRINTING FILE: %s\n\r", fname);
    
    while ( fgets (buf, sizeof(buf) - 1, fp) != NULL )
    {
      buf[sizeof(buf) - 1] = 0;
      debug_printf("%s", buf);
    }

    fclose (fp);
    
    debug_printf("\n\r");
  }
  else
  {
    debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
  }
}

////-------------------------------------------------------------------------------------
//void print_file_list_and_files(void)
//{
//  char *fnames = NULL;
//  int count = 0, i;
//  
//  uint16_t file_num;
//  uint32_t file_size;
//  
//  reset_file_list();
//    
//  debug_printf("FILE LIST START:\n\r");

//  do
//  {
//    fnames = realloc(fnames, (count+1) * (MAX_FNAME_SIZE+1) );
//    
//    if ( fnames )
//    {
//      if ( read_file_list( &fnames[count * (MAX_FNAME_SIZE+1)], &file_num, &file_size) >= 0 )
//      {
//        debug_printf("%05hu, %06u, %s\n\r", file_num, file_size, &fnames[count * (MAX_FNAME_SIZE+1)]);
//        count++;
//      }
//      else
//      {
//        break; // from do while
//      }
//    }
//    else
//    {
//      count = 0;
//      break; // from do while
//    }
//    
//    if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING ) taskYIELD();
//    
//  } while (1);
//  
//  debug_printf("FILE LIST END\n\r");
//  
//  for ( i = 0; i < count; i++ )
//  {
//    if ( strchr(&fnames[i*(MAX_FNAME_SIZE+1)],'.') == NULL )
//      printf_text_file(&fnames[i*(MAX_FNAME_SIZE+1)]);
//  }
//}

//-------------------------------------------------------------------------------------
void print_test_file (char *fname)
{
  int c;
  
  FILE *fp;
  
  fp = fopen (fname, "r");
  
  if (fp)
  {
    uint32_t i = 0;
    
    DEBUG_RAW_printf ("\nFILE: %s\n\r", fname);
    
    do
    {
      c = fgetc (fp);
      
      if (c >= 0)
      {
        if ((i % 100) == 0)
          DEBUG_RAW_printf("%05u: ", i);
        
        if (isprint (c))
        {
          DEBUG_RAW_printf ("%c",(char)c);
        }
        else
        {
          DEBUG_RAW_printf ("[0x%02X]", c);
        }
          
        if ((i % 100) == 99)
          DEBUG_RAW_printf ("\n\r", c);
        
        i++;
      }
      else
      {
        DEBUG_RAW_printf ("\nFILE: %s SIZE :%d\n\r", fname, i);
      }
    } while (c >= 0);

    if ((i % 100) != 0)
      DEBUG_RAW_printf ("\n\r");
    
    fclose (fp);
  }
  else
  {
    debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
  }
}

//-------------------------------------------------------------------------------------
void print_bin_file (char *fname, int block_len)
{
  int c;
  
  FILE *fp;
  
  fp = fopen (fname, "r");
  
  if (!block_len)
    block_len = 256;
  
  if (fp)
  {
    uint32_t idx = 0;
    
    DEBUG_RAW_printf ("\n\rFILE: %s\n\r", fname);
    
    do
    {
      c = fgetc (fp);
      
      if (c >= 0)
      {
        if (idx % block_len == 0)
          DEBUG_RAW_printf("\n\r%05u: ", idx / block_len);
      
        DEBUG_RAW_printf ("[%02X]", c);
        idx++;
      }
    } while (c >= 0);

    if ((idx % block_len) != 0)
      DEBUG_RAW_printf ("\n\r");
    
    DEBUG_RAW_printf ("FILE: %s SIZE :%d\n\r", fname, idx);    
    
    fclose (fp);
  }
  else
  {
    debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
  }
}

//-------------------------------------------------------------------------------------
void print_file_list (void)
{
  char fname [MAX_FNAME_SIZE+1];
  uint32_t file_num;
  uint32_t file_size;
  
//  fs_scan_media ();
  
  reset_file_list ();
    
//  DEBUG_RAW_printf ("FILE LIST START:\n\r");
  debug_printf ("FILE LIST START:\n\r");

  while (read_file_list (fname,&file_num,&file_size) >= 0)
  {
//    DEBUG_RAW_printf ("%05hu, %06u, %.*s\n\r", file_num, file_size, MAX_FNAME_SIZE, fname);
    debug_printf ("%05hu, %06u, %.*s\n\r", file_num, file_size, MAX_FNAME_SIZE, fname);
  }
  
//  DEBUG_RAW_printf ("FILE LIST END\n\r");
  debug_printf ("FILE LIST END\n\r");
}

//-------------------------------------------------------------------------------------
void remove_all (void)
{
  char fname [MAX_FNAME_SIZE+1];
  uint32_t file_num;
  uint32_t file_size;
  
//  fs_scan_media ();
  
  reset_file_list ();
    
//  DEBUG_RAW_printf ("FILE LIST START:\n\r");
  debug_printf ("FILE REMOVE ALL:\n\r");

  while (read_file_list (fname,&file_num,&file_size) >= 0)
  {
//    DEBUG_RAW_printf ("%05hu, %06u, %.*s\n\r", file_num, file_size, MAX_FNAME_SIZE, fname);
    debug_printf ("REMOVE %05hu, %06u, %.*s\n\r", file_num, file_size, MAX_FNAME_SIZE, fname);
    remove (fname);
    reset_file_list ();
  }
  
//  DEBUG_RAW_printf ("FILE LIST END\n\r");
  debug_printf ("FILE REMOVE END\n\r");
}


//-------------------------------------------------------------------------------------
#define FS_TEST_FILES         5
#define FS_TEST_SIZE          400000
#define FS_TEST_APPEND_SIZE   20000
#define FS_TEST_CLIP_SIZE     27000
#define FS_TEST_CLIP_SIZE_2   20000

//-------------------------------------------------------------------------------------
#define TEST 0

//-------------------------------------------------------------------------------------
void test_fs (void)
{
#ifndef RELEASE
#if 0
  fs_scan_media ();
#endif

#if 0
  test_clip ();
#endif
    
#if 0
  Format_fs();
#endif
  
#if 0
  remove_all();
#endif
  
#if TEST
  {
    char fname [MAX_FNAME_SIZE+1];
    int i, count;
    FILE *fp;
    
//    for ( i = FS_TEST_FILES; i < (FS_TEST_FILES + FS_TEST_FILES); i ++ )
    for (i = 0; i < (FS_TEST_FILES + FS_TEST_FILES); i ++)
    {
      sprintf (fname, "FILE_%d", i);
      
      debug_printf ("Removing %s\n\r",fname);

      remove (fname);
    }
    
    
  
//  test fs
//  write FS_TEST_FILES files of FS_TEST_SIZE

    print_file_list ();
  
    for (i = 0; i < FS_TEST_FILES; i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Writing %s (%d)\n\r", fname, fs_access (fname));

      fp = fopen (fname, "w");

      if (fp)
      {
//        c = ' ';
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c, fp) != c )
            if (fputc ('A',fp) == EOF)
            {
              debug_printf ("\t\t\t\tfputc error @ %d\t\t\t\t\r", count);
            }
            
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE) ;
        
        debug_printf ("\n\r");
        
        fclose (fp);
        
        int file_size = fs_access (fname);
        
        if (file_size != FS_TEST_SIZE)
        {
          debug_printf ("file size error %s: %d\n\r", fname, file_size);
        }
      }
      else
      {
        debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
      }
    }
    
    print_file_list ();
    //printf_file("FILE_0");

    for (i = 0; i < FS_TEST_FILES; i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Reading %s (%d)\n\r", fname, fs_access (fname));

      fp = fopen (fname, "r");

      if (fp)
      {
//        c = ' ';
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
            int c = fgetc (fp);
            
//            if ( fputc (c, fp) != c )
            if (c != 'A')
            {
              debug_printf ("\t\t\t\tfgetc error %02X @ %d\t\t\t\t\r", c, count);
            }
            
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE);
        
        debug_printf ("\n\r");
        
        fclose (fp);
      }
      else
      {
        debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
      }
    }
  }
#endif

#if TEST
//  rewrite 20 files of 20K again
  {
    char fname [MAX_FNAME_SIZE+1];
    int i, count;
    FILE *fp;

    for (i = 0; i < FS_TEST_FILES; i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Rewriting %s (%d)\n\r", fname, fs_access (fname));

      fp = fopen (fname, "w");

      if (fp)
      {
//        c = 0;
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c, fp) != c )
            if (fputc ('B',fp) == EOF)
            {
              debug_printf ("\t\t\t\tfputc error @ %d\t\t\t\t\r", count);
            }
            count++;
          }

//          c++;
        } while (count < FS_TEST_SIZE);
        
        debug_printf ("\n\r");
        
        fclose (fp);
        
        int file_size = fs_access (fname);
        
        if (file_size != FS_TEST_SIZE)
        {
          debug_printf ("file size error %s: %d\n\r", fname, file_size);
        }
      }
      else
      {
        debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
      }
    }

    print_file_list ();
//    printf_file("FILE_0");
    
    for (i = 0; i < FS_TEST_FILES; i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Reading %s (%d)\n\r", fname, fs_access (fname));

      fp = fopen (fname, "r");

      if (fp)
      {
//        c = ' ';
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c, fp) != c )
            if (fgetc (fp) != 'B')
            {
              debug_printf ("\t\t\t\tfgetc error @ %d\t\t\t\t\r", count);
            }
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE) ;
        
        debug_printf ("\n\r");
        
        fclose (fp);
      }
      else
      {
        debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
      }
    }
  }
#endif
    
#if TEST
//  rename 20 files
  {
    char fname [MAX_FNAME_SIZE + 1];
    char fname2 [MAX_FNAME_SIZE + 1];
    int i, count;
    FILE *fp;
    
    for (i = 0; i < FS_TEST_FILES; i ++)
    {
      sprintf (fname, "FILE_%d", i);
      sprintf (fname2, "FILE_%d", i + FS_TEST_FILES);

      debug_printf ("Renaming %s (%d) to %s\n\r", fname, fs_access (fname), fname2);

      rename (fname, fname2);
    }

    print_file_list ();
    
//  apend 20 files with 15K ( total 700K )
    for (i = FS_TEST_FILES; i < (FS_TEST_FILES + FS_TEST_FILES); i++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Appending %s (%d)\n\r", fname, fs_access (fname));

      fp = fopen (fname, "a");

      if (fp)
      {
//        c = 0;
        count = 0;

        fputs ("Appending from here ->", fp);

        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c, fp) != c )
            if (fputc ('C', fp) == EOF)
            {
              debug_printf ("\t\t\t\tfputc error @ %d\t\t\t\t\r",count);
            }
            count++;
          }

//          c++;
        } while (count < FS_TEST_APPEND_SIZE);
        
        debug_printf ("\n\r");
        
        fclose (fp);
        
        int file_size = fs_access (fname);
        
        if (file_size != (FS_TEST_SIZE + FS_TEST_APPEND_SIZE + strlen ("Appending from here ->")))
        {
          debug_printf ("file size error %s: %d\n\r", fname, file_size);
        }
      }
      else
      {
        debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
      }
    }

    print_file_list ();
//    printf_file("FILE_100");
    
    for (i = FS_TEST_FILES; i < (FS_TEST_FILES + FS_TEST_FILES); i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Reading %s, (%d)\n\r", fname, fs_access (fname));

      fp = fopen (fname, "r");

      if (fp)
      {
//        c = ' ';
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c, fp) != c )
            if (fgetc (fp) != 'B')
            {
              debug_printf ("fgetc error @ %d\r", count);
            }
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE) ;

        fseek(fp, FS_TEST_SIZE + strlen ("Appending from here ->"), SEEK_SET); /* Some platforms might not have rewind(), Oo */
        count = FS_TEST_SIZE + strlen ("Appending from here ->");
        
        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c,fp) != c )
            if (fgetc (fp) != 'C')
            {
              debug_printf ("\t\t\t\tfgetc error @ %d\t\t\t\t\r", count);
            }
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE + FS_TEST_APPEND_SIZE + strlen ("Appending from here ->")) ;
        
        debug_printf ("\n\r");
        
        fclose (fp);
      }
      else
      {
        debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
      }
    }
  }
#endif

    
//  open MAX_OPEN_FILE files for apending

    

#if TEST
//  fclip 20 files with 17K ( 260K )
  {
    char fname [MAX_FNAME_SIZE+1];
    int i, count;
    FILE *fp;
    
    for (i = FS_TEST_FILES; i < (FS_TEST_FILES + FS_TEST_FILES); i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Clipping %s (%d)\n\r", fname, fs_access (fname));

      fs_fclip (fname, FS_TEST_CLIP_SIZE);
      
      int file_size = fs_access (fname);
      
      if (file_size != (FS_TEST_SIZE + FS_TEST_APPEND_SIZE + strlen ("Appending from here ->") - FS_TEST_CLIP_SIZE))
      {
        debug_printf ("file size error %s: %d\n\r", fname, file_size);
      }
    }

    print_file_list ();
    
    sprintf (fname, "FILE_%d", FS_TEST_FILES);
    
    print_test_file (fname);
    
    for (i = FS_TEST_FILES; i < (FS_TEST_FILES + FS_TEST_FILES); i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Reading %s (%d)\n\r",fname, fs_access (fname));

      fp = fopen (fname, "r");

      if (fp)
      {
//        c = ' ';
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c,fp) != c )
            if (fgetc (fp) != 'B')
            {
              debug_printf ("\t\t\t\tfgetc error @ %d\t\t\t\t\r", count);
            }
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE - FS_TEST_CLIP_SIZE);

        char buf [22];
        
        fgets (buf, sizeof (buf), fp);
        
        if (strncmp (buf, "Appending from here ->", 22) != 0)
        {
          debug_printf ("\t\t\t\tfgets error @ %d\t\t\t\t\r", count);
        }
        
        count += strlen ("Appending from here ->");
        
        //fsetpos (fp, FS_TEST_SIZE - FS_TEST_CLIP_SIZE + strlen ("Appending from here ->"));
        //count = FS_TEST_SIZE - FS_TEST_CLIP_SIZE + strlen ("Appending from here ->");
        
        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c,fp) != c )
            if (fgetc (fp) != 'C')
            {
              debug_printf ("\t\t\t\tfgetc error @ %d\t\t\t\t\r", count);
            }
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE - FS_TEST_CLIP_SIZE + strlen ("Appending from here ->") + FS_TEST_APPEND_SIZE);
        
        debug_printf ("\n\r");
        
        fclose (fp);
      }
      else
      {
        debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
      }
    }
  }
#endif
    
#if TEST
//  fclip 20 files with 10K ( 30K )
  {
    char fname [MAX_FNAME_SIZE+1];
    int i, count;
    FILE *fp;
    
    for (i = FS_TEST_FILES; i < (FS_TEST_FILES + FS_TEST_FILES); i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Clipping %s (%d)\n\r", fname, fs_access (fname));

      fs_fclip (fname, FS_TEST_CLIP_SIZE_2);
      
      int file_size = fs_access (fname);
      
      if (file_size != (FS_TEST_SIZE + FS_TEST_APPEND_SIZE + strlen ("Appending from here ->") - FS_TEST_CLIP_SIZE - FS_TEST_CLIP_SIZE_2))
      {
        debug_printf ("file size error %s: %d\n\r", fname, file_size);
      }
    }

    print_file_list ();
    
    sprintf (fname, "FILE_%d", FS_TEST_FILES);
    
    print_test_file (fname);
    
    for (i = FS_TEST_FILES; i < (FS_TEST_FILES + FS_TEST_FILES); i ++)
    {
      sprintf (fname, "FILE_%d", i);

      debug_printf ("Reading %s, (%d)\n\r",fname, fs_access (fname));

      fp = fopen (fname, "r");

      if (fp)
      {
//        c = ' ';
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
//            if ( fputc (c,fp) != c )
            if (fgetc (fp) != 'B')
            {
              debug_printf ("\t\t\t\tfgetc error @ %d\t\t\t\t\r", count);
            }
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE - FS_TEST_CLIP_SIZE - FS_TEST_CLIP_SIZE_2);
        
        char buf [22];
        
        fgets (buf, sizeof (buf), fp);
        
        if (strncmp (buf, "Appending from here ->", 22) != 0)
        {
          debug_printf ("\t\t\t\tfgets error @ %d\t\t\t\t\r", count);
        }
        
        count += strlen ("Appending from here ->");
        
        //fsetpos (fp, FS_TEST_SIZE - FS_TEST_CLIP_SIZE + strlen ("Appending from here ->"));
        //count = FS_TEST_SIZE - FS_TEST_CLIP_SIZE + strlen ("Appending from here ->");
        
        do
        {
//          if ( isprint(c) )
          {
//            if ( !fputc(c,fp) )
            if (fgetc (fp) != 'C')
            {
              debug_printf ("\t\t\t\tfgetc error @ %d\t\t\t\t\r", count);
            }
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while (count < FS_TEST_SIZE - FS_TEST_CLIP_SIZE - FS_TEST_CLIP_SIZE_2 + strlen ("Appending from here ->") + FS_TEST_APPEND_SIZE);
        
        debug_printf ("\n\r");
        
        fclose (fp);        
      }
      else
      {
        debug_printf ("error opening file %s: %s\n\r", fname, print_pseffs_error ());
      }
    }
  }
#endif
  
#if 1
//  remove 20 files

  {
    char fname [MAX_FNAME_SIZE + 1];
    int i;

//    for ( i = FS_TEST_FILES; i < (FS_TEST_FILES + FS_TEST_FILES); i ++ )
    for (i = 0; i < (FS_TEST_FILES + FS_TEST_FILES); i ++)
    {
      sprintf (fname, "FILE_%d", i);
      
      debug_printf ("Removing %s\n\r",fname);

      remove (fname);
    }
    
//    print_file_list();
  }
#endif
  
  print_file_list ();

#if 0
  int log_size = fs_access (Log_File);
  
  if (log_size > (4 * 1024))
    fs_fclip (Log_File, log_size - (3 * 1024));
#endif
  
#if 0
  print_text_file (Log_File);
#endif
  
#if 0
  remove (Log_File);
#endif

#if 0
  print_text_file (Gprs_File);
#endif

#if 0
  remove (Stats_File);
#endif
#endif
}

