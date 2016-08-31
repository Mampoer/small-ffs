//-------------------------------------------------------------------------------------
#include "application.h"
#include "fs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//#include "trcUser.h"
//#include "trcConfig.h"
//#include "trcHardwarePort.h"

//-------------------------------------------------------------------------------------
void DEBUG_printf( const char *ptr, ... );

//-------------------------------------------------------------------------------------
#define debug_printf(...)       DEBUG_printf(__VA_ARGS__)

//-------------------------------------------------------------------------------------
xTaskHandle Task_A_Handle = NULL;

//-------------------------------------------------------------------------------------
static SemaphoreHandle_t xMutex   = NULL;
 
//-------------------------------------------------------------------------------------
void printf_text_file(const char *fname)
{
  char buf[128];
  
  FS_FILE *fp;
  
  fp = fs_fopen( fname, 'r' );
  
  if ( fp )
  {
    debug_printf("\nPRINTING FILE: %s\n", fname);
    
    while ( fs_fgets(buf, sizeof(buf) - 1, fp) > 0 )
    {
      buf[sizeof(buf) - 1] = 0;
      debug_printf("%s", buf);
    }

    fs_fclose(fp);
    
    debug_printf("\n");
  }
}

//-------------------------------------------------------------------------------------
void print_file_list_and_files(void)
{
  char *fnames = NULL;
  int count = 0, i;
  
  uint16_t file_num;
  uint32_t file_size;
  
  reset_file_list();
    
  debug_printf("FILE LIST START:\n");

  do
  {
    fnames = realloc(fnames, (count+1) * (MAX_FNAME_SIZE+1) );
    
    if ( fnames )
    {
      if ( read_file_list( &fnames[count * (MAX_FNAME_SIZE+1)], &file_num, &file_size) >= 0 )
      {
        debug_printf("%05hu, %06u, %s\n", file_num, file_size, &fnames[count * (MAX_FNAME_SIZE+1)]);
        count++;
      }
      else
      {
        break; // from do while
      }
    }
    else
    {
      count = 0;
      break; // from do while
    }
    
    if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING ) taskYIELD();
    
  } while (1);
  
  debug_printf("FILE LIST END\n");
  
  for ( i = 0; i < count; i++ )
  {
    printf_text_file(&fnames[i*(MAX_FNAME_SIZE+1)]);
  }
}

//-------------------------------------------------------------------------------------
void printf_file(char *fname)
{
  int c;
  
  FS_FILE *fp;
  
  fp = fs_fopen( fname, 'r' );
  
  if ( fp )
  {
    uint32_t i = 0;
    
    debug_printf("\nFILE: %s\n", fname);
    
    do
    {
      c = fs_fgetc(fp);
      
      if ( c >= 0 )
      {
        if ( (i % 100) == 0 )
          debug_printf("%05u: ", i);
        
        if ( isprint(c) )
          debug_printf("%c",(char)c);
        else
          debug_printf("[0x%02X]",c);
          
        if ( (i % 100) == 99 )
          debug_printf("\n",c);
        
        i++;
      }
      
      if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING ) taskYIELD();
      
    } while ( c >= 0 );

    if ( (i % 100) != 0 )
      debug_printf("\n");
    
    fs_fclose(fp);
  }
}

//-------------------------------------------------------------------------------------
void print_file_list(void)
{
  char fname[MAX_FNAME_SIZE+1];
  uint16_t file_num;
  uint32_t file_size;
  
  reset_file_list();
    
  debug_printf("FILE LIST START:\n");
    
  while (read_file_list(fname,&file_num,&file_size) >= 0)
    debug_printf("%05hu, %06u, %s\n", file_num, file_size, fname);
  
  debug_printf("FILE LIST END\n");
}

//-------------------------------------------------------------------------------------
void test_fs(void)
{
  char fname[MAX_FNAME_SIZE+1];
  char fname2[MAX_FNAME_SIZE+1];
  int i, count;
  
  FS_FILE *fp;
  
//  Format_fs();
  
  {
//  test fs
//  write 20 files of 10K  ( 200K )

    print_file_list();
  
    for ( i = 0; i < 20; i ++ )
    {
      sprintf( fname, "FILE_%d", i );

      debug_printf("Writing %s\n",fname);

      fp = fs_fopen( fname, 'w' );

      if ( fp )
      {
//        c = ' ';
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
//            if ( !fs_fputc(c,fp) )
            if ( !fs_fputc('A',fp) )
            {
              debug_printf("fputc error @ %d\n",count);
              while(1);
            }
            count++;
          }

//          c++;
          
//          if ( c > '~' ) c = ' ';
          
        } while ( count < 10000 ) ;
        
        fs_fclose(fp);
      }
    }
    
    print_file_list();
    
    printf_file("FILE_0");
    
//  rewrite 20 files of 10K again
    for ( i = 0; i < 20; i ++ )
    {
      sprintf( fname, "FILE_%d", i );

      debug_printf("Rewriting %s\n",fname);

      fp = fs_fopen( fname, 'w' );

      if ( fp )
      {
//        c = 0;
        count = 0;

        do
        {
//          if ( isprint(c) )
          {
//            if ( !fs_fputc(c,fp) )
            if ( !fs_fputc('B',fp) )
            {
              debug_printf("fputc error @ %d\n",count);
              while(1);
            }
            count++;
          }

//          c++;
        } while ( count < 10000 ) ;
        fs_fclose(fp);
      }
    }

    print_file_list();
    
    printf_file("FILE_0");
    
//  rename 20 files
    for ( i = 0; i < 20; i ++ )
    {
      sprintf( fname, "FILE_%d", i );
      sprintf( fname2, "FILE_%d", i+100 );

      debug_printf("Renaming %s to %s\n", fname, fname2);

      fs_rename( fname, fname2 );
    }

    print_file_list();
    
//  apend 20 files with 5K ( total 300K )
    for ( i = 100; i < 120; i ++ )
    {
      sprintf( fname, "FILE_%d", i );

      debug_printf("Appending %s\n",fname);

      fp = fs_fopen( fname, 'a' );

      if ( fp )
      {
//        c = 0;
        count = 0;

        fs_fputs("Appending from here ->",fp);

        do
        {
//          if ( isprint(c) )
          {
//            if ( !fs_fputc(c,fp) )
            if ( !fs_fputc('C',fp) )
            {
              debug_printf("fputc error @ %d\n",count);
              while(1);
            }
            count++;
          }

//          c++;
        } while ( count < 5000 ) ;
        fs_fclose(fp);
      }
    }

    print_file_list();
    
    printf_file("FILE_100");
    
//  open MAX_OPEN_FILE files for apending

    

//  fclip 20 files with 1K ( 280K )
    for ( i = 100; i < 120; i ++ )
    {
      sprintf( fname, "FILE_%d", i );

      debug_printf("Clipping %s\n",fname);

      fs_fclip (fname, 1000);
    }

    print_file_list();
    
    printf_file("FILE_100");
    
//  remove 20 files
    for ( i = 100; i < 120; i ++ )
    {
      sprintf( fname, "FILE_%d", i );

      debug_printf("Removing %s\n",fname);

      fs_remove( fname );
    }
    
    print_file_list();
  }
}

//-------------------------------------------------------------------------------------
void init_app(void)
{
 	//vTraceInitTraceData();
	
	//if (! uiTraceStart() )
	//{
	//		vTraceConsoleMessage("Could not start recorder!");
	//}

  fs_scan_media();
                                                                                          
  test_fs();
  
  print_file_list_and_files();
                                                                                          
  xMutex = xSemaphoreCreateMutex();
  
  xTaskCreate(task_A, "A", configMINIMAL_STACK_SIZE, NULL, TASK_A_PRIO, Task_A_Handle);

  /* Start scheduler */
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */
}

//-------------------------------------------------------------------------------------
void task_A(void * pvParameters)
{
  while(1)
  {
  }
}
    
//-------------------------------------------------------------------------------------
void watchdog_reset_callback(void)
{
}

//-------------------------------------------------------------------------------------
//void Format_Flash_Callback(void)
//{
//  debug_printf("Formatting\n\r");
//  Format_fs();                  // format flash device for this file system
//}

//-------------------------------------------------------------------------------------
void Format_Progress_Callback(uint16_t page_number, uint16_t end_page)
{
  if ( page_number % 100 == 0 )
    debug_printf("Formatting %hu of %hu\n\r", page_number, end_page);
}

//-------------------------------------------------------------------------------------
void fs_error_callback (const char *err)
{
  debug_printf("FS CRITICAL ERROR! %s\n\r",err);
  while(1) {}; // let watch dog reset
}

//-------------------------------------------------------------------------------------
void scan_fs_callback(void)
{
  debug_printf("Scan fs Callback\n\r");

  //scan_port = putchar_func_pointer;
  
  //menu_scan();
}

//-------------------------------------------------------------------------------------
void scan_start_callback(uint16_t file_system_start, uint16_t file_system_end)
{
  if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
  {
    vTaskSuspendAll();
  }
  
  debug_printf("Scanning media from %hu to %hu\n\r",file_system_start, file_system_end);
}

//-------------------------------------------------------------------------------------
void scan_integrity_fail_callback(uint16_t file_num, char *filename)
{
  debug_printf("Integrity check for file %hu %s failed - removing\n\r", file_num, filename );
}

//-------------------------------------------------------------------------------------
void scan_file_info_callback(uint16_t file_num, char *filename, uint16_t page, uint16_t link_num)
{
  debug_printf( "File %hu %s, Page @ %hu - link number %hu\n\r", file_num, filename, page, link_num );
}

//-------------------------------------------------------------------------------------
void scan_dead_page_callback(uint16_t page)
{
  debug_printf("Dead page - %hu\n\r",page);
}

//-------------------------------------------------------------------------------------
void scan_fragment_callback(uint16_t file_num, uint16_t page)
{
  debug_printf("Fragment found of File num %hu, @ page %hu\n\r",file_num, page);
}

//-------------------------------------------------------------------------------------
void scan_page_erased_callback(uint16_t page)
{
  debug_printf("Page %hu erased\n\r",page);
}

//-------------------------------------------------------------------------------------
void scan_end_callback(uint16_t dead_pages)
{
  debug_printf("scanning done - %hu dead pages found\n\r", dead_pages);
  
  if ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED )
  {
    xTaskResumeAll();
  }
}

//-------------------------------------------------------------------------------------
void Flash_Fail_Callback(void)
{
  debug_printf("Flash Fail!\n\r");
  while(1) {}; // let watch dog reset
}

//-------------------------------------------------------------------------------------
void Delay(int delay_ms)
{
  if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
  {
    vTaskDelay(delay_ms);
  }
  else
  {
//    int timer = ValueTimerForRunTimeStats() + (delay_ms * 10);
//    while ( ValueTimerForRunTimeStats() < timer );
  }
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
bool fs_mutex_take(void)
{
  if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
  {
    if( xMutex != NULL )
    {
      // See if we can obtain the mutex.  If the mutex is not available
      // wait 10 ticks to see if it becomes free.
      if( xSemaphoreTake( xMutex, ( TickType_t ) 100 ) != pdTRUE )
      {
        return false;
      }
    }      
  }
  
  return true;
}

//-------------------------------------------------------------------------------------
void fs_mutex_give(void)
{
  if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
  {
    if( xMutex != NULL )
    {
      xSemaphoreGive( xMutex );
    }
  }
}

//-------------------------------------------------------------------------------------
void HardFault_Handler(void)
{
  NVIC_SystemReset();
  while(1);
}

//-------------------------------------------------------------------------------------
void assert_failed(uint8_t* file, uint32_t line)
{
  debug_printf("assert_failed %s line %d\n", file, line);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
