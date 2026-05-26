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

#include <string.h>
#include <stdio.h>

#include "flash_driver.h"

//-------------------------------------------------------------------------------------
// debug_printf: by default a no-op. Define FLASH_DRIVER_DEBUG to route to stdio printf,
// or provide your own implementation (e.g. UART) by overriding this macro.
//-------------------------------------------------------------------------------------
#ifndef debug_printf
  #ifdef FLASH_DRIVER_DEBUG
    #define debug_printf(...) do { printf("[FLASH %d] ", __LINE__); printf(__VA_ARGS__); } while (0)
  #else
    #define debug_printf(...) ((void)0)
  #endif
#endif

//-------------------------------------------------------------------------------------
#define flash_size 				(flash_sector_size * flash_sectors)
#define sector_num(addr)	(addr / flash_sector_size)

//-------------------------------------------------------------------------------------
typedef enum {
  FLASH_TYPE_UNDEF = 0,
  FLASH_TYPE_ADESTO_AT25DL161,
  FLASH_TYPE_ADESTO_AT25SF161,
  FLASH_TYPE_ADESTO_AT25SL128,
  FLASH_TYPE_MICRON_MT25QL256,
  FLASH_TYPE_CYPRES_S25FL127S,
} flash_t;

flash_t flash = FLASH_TYPE_UNDEF;

////-------------------------------------------------------------------------------------
//#define FLASHSTART		              0x80000000
//#define BYTES_PER_BLOCK             0x20000UL
//#define MAX_WORD_CHECK_COUNT        10                        /* How many time do we try checking a word    */
//#define MAX_CHIP_UNPROTECT_COUNT    10                        /* How many times to we try unprotect a chip  */
//#define MAX_BUFFER_WRITE_COUNT      20                        /* How many times do we try buffer & write    */
//#define MAX_WORD_WRITE_COUNT        100                       /* How many times do we try word write        */
//#define M25P16_ID_MANUFACTURER        0x20
//#define M25P16_ID_DEVICE_HI           0x20
//#define M25P16_ID_DEVICE_LO           0x15

/* SPI vendor IDs */
#define VENDOR_ID_ADESTO			        0x1f
#define VENDOR_ID_AMIC				        0x37
#define VENDOR_ID_ATMEL				        0x1f
#define VENDOR_ID_EON				          0x1c
#define VENDOR_ID_GIGADEVICE			    0xc8
#define VENDOR_ID_MACRONIX			      0xc2
#define VENDOR_ID_SPANSION			      0x01
#define VENDOR_ID_SST				          0xbf
#define VENDOR_ID_STMICRO			        0x20
#define VENDOR_ID_WINBOND			        0xef
#define VENDOR_ID_CYPRESS			        0x01
#define VENDOR_ID_MICRON			        0x20

// AT25SF161 flash identification
#define ADESTO_AT25DL161_ID_DEVICE    0x1f4603
#define ADESTO_AT25SF161_ID_DEVICE    0x1f8601
#define ADESTO_AT25SL128_ID_DEVICE    0x1f4218
#define MICRON_MT25QL256_ID_DEVICE    0x20BA19
#define CYPRES_S25FL127S_ID_DEVICE    0x012018

#define M25P16_TIMEOUT_PAGE           6       // timeout [ms] for page write
#define M25P16_TIMEOUT_SECTOR         3000    // timeout [ms] for sector erase
#define M25P16_TIMEOUT_BULK           20000   // timeout [ms] for bulk erase

#define M25P16_INIT_RETRY             3       // retry count for flash init

//-------------------------------------------------------------------------------------
#define     READ_ID                   0x9F
#define     WRITE_COMMAND             0x02  // 0.7 ms
#define     READ_COMMAND              0x03
#define     WRDI_COMMAND              0x04
#define     READ_STATUS_REG           0x05
#define     WREN_COMMAND              0x06
#define     STATUS2_COMMAND           0x07
#define     BANK_READ_COMMAND         0x16
#define     PPBL_READ_COMMAND         0xA7
#define     CONFIG_COMMAND            0x35
#define     ERASE_4K_COMMAND          0x20  // 4K 70ms
#define     ERASE_64K_COMMAND         0xD8  // 64K 170ms
#define     READ_FLAG_STATUS_REG      0x70

#define     CLEAR_STATUS_COMMAND      0x30

#define     UNPROTECT_COMMAND         0x39

/* at25dfxx-specific commands */
//#define CMD_AT25DF_WRSR		0x01	/* Write Status Register */
//#define CMD_AT25DF_DP		0xb9	/* Deep Power-down */
//#define CMD_AT25DF_RES		0xab	/* Release from DP, and Read Signature */

//-------------------------------------------------------------------------------------
uint32_t    flash_sectors             = 0;
uint32_t    flash_page_size           = 0;
uint32_t    flash_pages_per_sector    = 0;
uint32_t    file_system_start_page    = 0;
uint32_t    file_system_end_page      = 0;
uint32_t    flash_max_buf             = 0;

//-------------------------------------------------------------------------------------
// Flash Functions
//-------------------------------------------------------------------------------------
static void setupFlash3ByteAddressCommand (uint8_t *address_buf, uint8_t command, uint32_t address)
{
  union 
  {
    uint32_t addr32;
    uint8_t  addr8[4];
  } addr;
  
  // byte pos
  addr.addr32 = address;
  
//    address_buf[0] = command;
//    address_buf[1] = (addr.addr32>>16) & 0xff; // little endian
//    address_buf[2] = (addr.addr32>>8) & 0xff;
//    address_buf[3] = addr.addr32 & 0xff;
  
  address_buf[0] = command;
  address_buf[1] = addr.addr8[2]; // little endian
  address_buf[2] = addr.addr8[1];
  address_buf[3] = addr.addr8[0];
}

//-------------------------------------------------------------------------------------
static void setupFlash4ByteAddress (uint8_t *address_buf, uint32_t address)
{
  union 
  {
    uint32_t addr32;
    uint8_t  addr8[4];
  } addr;
  
  // byte pos
  addr.addr32 = address;
  
//    address_buf[0] = (addr.addr32>>24) & 0xff;
//    address_buf[1] = (addr.addr32>>16) & 0xff; // little endian
//    address_buf[2] = (addr.addr32>>8) & 0xff;
//    address_buf[3] = addr.addr32 & 0xff;
  
  address_buf[0] = addr.addr8[3];
  address_buf[1] = addr.addr8[2]; // little endian
  address_buf[2] = addr.addr8[1];
  address_buf[3] = addr.addr8[0];
}

//-------------------------------------------------------------------------------------
#define STATUS_BUZY                   = 0x01
#define STATUS_WRITE_ENABLE           = 0x02
#define STATUS_BP_0                   = 0x04
#define STATUS_BP_1                   = 0x08
#define STATUS_BP_2                   = 0x10
#define STATUS_PROTECTED_BOTTOM       = 0x20
#define STATUS_WRITE_STATUS_ENABLE    = 0x80

//-------------------------------------------------------------------------------------
static uint8_t read_status (void)
{
  volatile uint8_t status = 0;

  Select_Flash ();
  spi_send_byte (READ_STATUS_REG);
  status = spi_read_byte();
  Deselect_Flash ();
  
  return status;
}

//-------------------------------------------------------------------------------------
#define FLASG_STATUS_READY              = 0x80
#define FLASG_STATUS_ERASE_SUSPEND      = 0x40
#define FLASG_STATUS_ERASE_ERROR        = 0x20
#define FLASG_STATUS_PROGRAM_ERROR      = 0x10
#define FLASG_STATUS_PROGRAM_SUSPEND    = 0x04
#define FLASG_STATUS_PROTECT_ERROR      = 0x02

//-------------------------------------------------------------------------------------
//static uint8_t read_flag_status (void)
//{
//  volatile uint8_t status = 0;

//  Select_Flash ();
//  spi_send_byte (READ_STATUS_REG);
//  status = spi_read_byte();
//  Deselect_Flash ();
//  
//  return status;
//}

//-------------------------------------------------------------------------------------
static uint8_t read_status2 (void)
{
  volatile uint8_t status = 0;

  Select_Flash ();
  spi_send_byte (STATUS2_COMMAND);
  status = spi_read_byte();
  Deselect_Flash ();
  
  return status;
}

//-------------------------------------------------------------------------------------
static uint8_t read_config (void)
{
  volatile uint8_t status = 0;

  Select_Flash ();
  spi_send_byte (CONFIG_COMMAND);
  status = spi_read_byte();
  Deselect_Flash ();
  
  return status;
}

//-------------------------------------------------------------------------------------
static uint8_t read_bank_select (void)
{
  volatile uint8_t status = 0;

  Select_Flash ();
  spi_send_byte (BANK_READ_COMMAND);
  status = spi_read_byte();
  Deselect_Flash ();
  
  return status;
}

//-------------------------------------------------------------------------------------
static uint8_t read_ppb_lock (void)
{
  volatile uint8_t status = 0;

  Select_Flash ();
  spi_send_byte (PPBL_READ_COMMAND);
  status = spi_read_byte();
  Deselect_Flash ();
  
  return status;
}

//-------------------------------------------------------------------------------------
static uint8_t flash_busy_delay (void)
{
  volatile uint8_t status = 0;
  
  flash_driver_blocking_callback_start ();

  do
  {
    status = read_status ();
    
    if ((status & 0x01) == 0x01)
    {
      flash_driver_blocking_callback ();
    }
    else
      break;
    
    if (flash == FLASH_TYPE_CYPRES_S25FL127S)
    {
  //  bit 6 P_ERR Programming Error Occurred Volatile   Read only   1 = Error occurred    0 = No Error
  //  bit 5 E_ERR Erase Error Occurred Volatile         Read only   1 = Error occurred    0 = No Error
      
      if (status & 0x60) // error
      {
        Select_Flash ();
        spi_send_byte (CLEAR_STATUS_COMMAND);
        Deselect_Flash ();
        
        debug_printf ("Flash status error cleared\n\r");
      }
    }
    
  } while(1);
  
  flash_driver_blocking_callback_end ();
  
  return status;
}

//-------------------------------------------------------------------------------------
static void Flash_WRITE_ENABLE (void)
{
  flash_busy_delay ();    // DELAY UNTIL NOT BUZY
  Select_Flash ();
  spi_send_byte (WREN_COMMAND);
  Deselect_Flash ();
  
//  debug_printf ("Flash write enable\n\r");
}

//-------------------------------------------------------------------------------------
static void Flash_WRITE_DISABLE (void)
{
  flash_busy_delay ();    // DELAY UNTIL NOT BUZY
  Select_Flash ();
  spi_send_byte (WRDI_COMMAND);
  Deselect_Flash ();
  
//  debug_printf ("Flash write disable\n\r");
}

//-------------------------------------------------------------------------------------
//static void Flash_ENABLE_WRITE_STATUS_REGISTER (void)
//{
//  flash_busy_delay ();    // DELAY UNTIL NOT BUZY
//  Select_Flash ();
//  spi_send_byte(0x50);		/* enable writing to the status register */
//  Deselect_Flash ();
//  
//  debug_printf ("Flash write status enable\n\r");
//}

//-------------------------------------------------------------------------------------
//static void Flash_WRITE_STATUS_REGISTER_1 (uint8_t byte)
//{
//  flash_busy_delay ();    // DELAY UNTIL NOT BUZY
//  Select_Flash ();
//  spi_send_byte(0x01);		/* select write to status register */
//  spi_send_byte(byte);		/* data that will change the status of BPx or BPL (only bits 2,3,4,5,7 can be written) */
//  Deselect_Flash ();
//  
//  debug_printf ("Flash write status 1 enable\n\r");
//}

//-------------------------------------------------------------------------------------
//static void Flash_WRITE_STATUS_REGISTER_2 (uint8_t byte)
//{
//  flash_busy_delay ();    // DELAY UNTIL NOT BUZY
//  Select_Flash ();
//  spi_send_byte(0x31);		/* select write to status register */
//  spi_send_byte(byte);		/* data that will change the status of BPx or BPL (only bits 2,3,4,5,7 can be written) */
//  Deselect_Flash ();
//  
//  debug_printf ("Flash write status 2 enable\n\r");
//}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void unprotect_sector (uint32_t addr)
{
  uint8_t address_buf [4];

  uint8_t status = flash_busy_delay ();    // DELAY UNTIL NOT BUZY
  
  if (flash == FLASH_TYPE_CYPRES_S25FL127S)
  {
    
    flash_busy_delay ();  
    
    setupFlash4ByteAddress (address_buf, addr);
    
    Select_Flash ();
    spi_send_byte (0xE0); //DYB Read
    spi_send_byte (address_buf [0]);
    spi_send_byte (address_buf [1]);
    spi_send_byte (address_buf [2]);
    spi_send_byte (address_buf [3]);
    status = spi_read_byte ();
    Deselect_Flash ();
    
    if (status != 0xFF)
    {
      if ((status & 0x02) != 0x02)
      {
        Flash_WRITE_ENABLE();
      }
      
      flash_busy_delay ();  
      
      Select_Flash ();
      spi_send_byte (0xE1); //DYB Write
      spi_send_byte (address_buf [0]);
      spi_send_byte (address_buf [1]);
      spi_send_byte (address_buf [2]);
      spi_send_byte (address_buf [3]);
      spi_send_byte (0xFF);
      Deselect_Flash ();
      
      status = flash_busy_delay ();    // DELAY UNTIL NOT BUZY
      
      Select_Flash ();
      spi_send_byte (0xE0); //DYB Read
      spi_send_byte (address_buf [0]);
      spi_send_byte (address_buf [1]);
      spi_send_byte (address_buf [2]);
      spi_send_byte (address_buf [3]);
      status = spi_read_byte();
      Deselect_Flash ();
      
      debug_printf ("unprotect sector %u (%06X) DYB bits (%02hhX)\n\r", sector_num(addr), addr, status);
    }
    
    flash_busy_delay ();
    
    Select_Flash ();
    spi_send_byte (0xE2); //PPB Read
    spi_send_byte (address_buf [0]);
    spi_send_byte (address_buf [1]);
    spi_send_byte (address_buf [2]);
    spi_send_byte (address_buf [3]);
    status = spi_read_byte();
    Deselect_Flash ();
    
    if (status != 0xFF)
    {
      if ((status & 0x02) != 0x02)
      {
        Flash_WRITE_ENABLE();
      }
      
      flash_busy_delay ();

      Select_Flash ();
      spi_send_byte (0xE4); //PPB Erase
      Deselect_Flash ();
      
      flash_busy_delay ();
      
      Select_Flash ();
      spi_send_byte (0xE2); //PPB Read
      spi_send_byte (address_buf [0]);
      spi_send_byte (address_buf [1]);
      spi_send_byte (address_buf [2]);
      spi_send_byte (address_buf [3]);
      status = spi_read_byte();
      Deselect_Flash ();
      
      debug_printf ("unprotect sector %u (%06X) DYB bits (%02hhX)\n\r", sector_num(addr), addr, status);
    }
  }
  else
  {
    if ((status & 0x02) != 0x02)
    {
      Flash_WRITE_ENABLE();
    }
    
    //debug_printf (("flash_driver_erase_sector_address @ %hu\n\r", sector_num));
    
    setupFlash3ByteAddressCommand (address_buf, UNPROTECT_COMMAND, addr);
    
    Select_Flash ();
    Write_Data_Page (address_buf, 4);
    Deselect_Flash ();
    
    status = flash_busy_delay ();
    
//    debug_printf ("unprotect_sector %u (%02hhX)\n\r", sector_num, status);
  }
}

//-------------------------------------------------------------------------------------
void blocking_ms_delay (int delay);

//-------------------------------------------------------------------------------------
void flash_driver_erase_sector_address (uint32_t addr, bool erase64k)
{
  if (addr < flash_size)
  {
    unprotect_sector (addr);
    
    while ((flash_busy_delay () & 0x02) != 0x02)
    {
      Flash_WRITE_ENABLE();
    }

    uint8_t address_buf[4];
    
    if (flash == FLASH_TYPE_CYPRES_S25FL127S)
      erase64k = true;
    
    if (erase64k)
    {
      setupFlash3ByteAddressCommand (address_buf, ERASE_64K_COMMAND, addr);
      debug_printf ("Flash_Erase_64K_Sector %u (%06X) of %u size (%u)\n\r", sector_num(addr), addr, flash_sectors, flash_sector_size);
    }
    else
    {
      setupFlash3ByteAddressCommand (address_buf, ERASE_4K_COMMAND, addr);
      debug_printf ("Flash_Erase_4K_Sector %u (%06X) of %u size (%u)\n\r", sector_num(addr), addr, flash_sectors, flash_sector_size);
    }

    Select_Flash ();
    Write_Data_Page (address_buf, 4);
    Deselect_Flash ();
    
//    if (flash == FLASH_TYPE_CYPRES_S25FL127S)
//      blocking_ms_delay (10);
    
    flash_busy_delay ();

    Flash_WRITE_DISABLE();
  }
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void flash_driver_read (uint32_t address, uint8_t *data, uint32_t len)
{
  if (address < flash_size)
  {
    uint8_t address_buf[4];
    
    (void)flash_busy_delay ();    // DELAY UNTIL NOT BUZY
    
    setupFlash3ByteAddressCommand (address_buf, READ_COMMAND, address);
    
    Select_Flash ();
    Write_Data_Page (address_buf, 4);
    Read_Data_Page (data, len);
    Deselect_Flash ();
  }
}

//-------------------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>

//-------------------------------------------------------------------------------------
int flash_driver_write (uint32_t address, const uint8_t *data, uint32_t len, bool verify)
{
  if (address < flash_size)
  {
    int ret = 0;

    unprotect_sector (address);
    
    while (data && len)
    {
//      if (flash == FLASH_TYPE_CYPRES_S25FL127S)
//        blocking_ms_delay (10);
            
      uint8_t status = flash_busy_delay ();    // DELAY UNTIL NOT BUZY

      if ((status & 0x02) != 0x02)
      {
        Flash_WRITE_ENABLE();
      }
      
      uint32_t to_boundry = flash_max_buf - (address % flash_max_buf);

      uint32_t write_len = len;

      if (write_len > to_boundry)
      {
        debug_printf ("flash_driver_write boudry hit @ addr %u (%04X), wr len %u, to boundry %u\n\r"
                              , address, address, write_len, to_boundry);
        
        write_len = to_boundry;
      }
      

      uint8_t address_buf[4];

    //  Select_Flash();
    //  spi_send_byte(0x80);
    //  Deselect_Flash();
    
      //debug_printf(("Write_Sector_Info @ %hu\n\r", sector_num));
      setupFlash3ByteAddressCommand (address_buf, WRITE_COMMAND, address);

      
  //    flash_busy_delay ();  
      
  //    blocking_ms_delay (2);
      status = flash_busy_delay ();    // DELAY UNTIL NOT BUZY

      Select_Flash ();
      Write_Data_Page (address_buf, 4);
      Write_Data_Page (data, write_len);
      Deselect_Flash();
      
//      if (flash == FLASH_TYPE_CYPRES_S25FL127S)
//        blocking_ms_delay (10);
      
      if (verify)
      {
        uint8_t *test_data = malloc (write_len);
        
        if (test_data)
        {
          flash_driver_read (address, test_data, write_len);
          
          if (memcmp (data, test_data, write_len) != 0)
          {
            ret = -1;
            
            uint8_t status1 = read_status ();
            uint8_t status2 = read_status2 ();
            uint8_t status3 = read_config ();
            uint8_t status4 = read_bank_select ();
            uint8_t status5 = read_ppb_lock ();
            
            
            debug_printf ("flash_driver_write failed @ addr %u (%04X), wr len %u, to boundry %u"
                              ", stat %02hhX %02hhX %02hhX %02hhX %02hhX\n\r"
                                  , address, address, write_len, to_boundry
                                    , status1, status2, status3, status4, status5);
            
            for (int i = 0; i < write_len; i++)
            {
              if (data[i] != test_data[i])
              {
                debug_printf ("1: %03u, [%02X](%02X)\n\r", i, data[i], test_data[i]);
              }
            }
            
            flash_driver_read (address, test_data, write_len);

            for (int i = 0; i < write_len; i++)
            {
              if (data[i] != test_data[i])
              {
                debug_printf ("2: %03u, [%02X](%02X)\n\r", i, data[i], test_data[i]);
              }
            }
          }

          free (test_data);
        }
      }
      
      address += write_len;
      data += write_len;
      len -= write_len;
    }
    
    return ret;
  }
  
  return -1;
}

//-------------------------------------------------------------------------------------
static void Read_Flash_ID (uint8_t *id, int size)
{
  flash_busy_delay ();    // DELAY UNTIL NOT BUZY
  
  Select_Flash ();
  spi_send_byte (READ_ID);
  Read_Data_Page (id, size);
  Deselect_Flash ();
  
  if (size == 1) debug_printf ("Flash device id %02hX\n\r"                              , id [0]);
  if (size == 2) debug_printf ("Flash device id %02hX %02hX\n\r"                        , id [0], id [1]);
  if (size == 3) debug_printf ("Flash device id %02hX %02hX %02hX\n\r"                  , id [0], id [1], id [2]);
  if (size == 4) debug_printf ("Flash device id %02hX %02hX %02hX %02hX\n\r"            , id [0], id [1], id [2], id [3]);
  if (size == 5) debug_printf ("Flash device id %02hX %02hX %02hX %02hX %02hX\n\r"      , id [0], id [1], id [2], id [3], id [4]);
  if (size == 6) debug_printf ("Flash device id %02hX %02hX %02hX %02hX %02hX %02hX\n\r", id [0], id [1], id [2], id [3], id [4], id [5]);
}

//-------------------------------------------------------------------------------------
bool flash_driver_init (void)
{
  if (!file_system_end_page)
  {
    uint8_t status;

    uint8_t idbuf [5] = {0};

    hw_init_flash ();
    
    status = flash_busy_delay();
    
    debug_printf ("flash status %02hhX\n\r", status);

    if ( status & 0x40 )
    {
      Flash_WRITE_DISABLE ();
      
//      Flash_EWSR ();
//      Flash_WRSR1 (0x00);
//      Flash_WRSR2 (0x10);
//      
//      flash_busy_delay ();    // DELAY UNTIL NOT BUZY
//      Select_Flash ();
//      spi_send_byte(0xF0);		/* RESET */
//      spi_send_byte(0xD0);		/* Confirm */
//      Deselect_Flash ();
    }
    
    Read_Flash_ID (idbuf, sizeof (idbuf));
    
    uint32_t id = (idbuf [0] * 0x10000) + (idbuf [1] * 0x100) + idbuf [2];
    
    switch (id)
    {
      case ADESTO_AT25SF161_ID_DEVICE: flash = FLASH_TYPE_ADESTO_AT25SF161;   debug_printf ("AT25SF161\n\r");      break;
      case ADESTO_AT25DL161_ID_DEVICE: flash = FLASH_TYPE_ADESTO_AT25DL161;   debug_printf ("AT25DL161\n\r");      break;
      case ADESTO_AT25SL128_ID_DEVICE: flash = FLASH_TYPE_ADESTO_AT25SL128;   debug_printf ("AT25SL128\n\r");      break;
      case MICRON_MT25QL256_ID_DEVICE: flash = FLASH_TYPE_MICRON_MT25QL256;   debug_printf ("MT25QL256\n\r");      break;
      case CYPRES_S25FL127S_ID_DEVICE: flash = FLASH_TYPE_CYPRES_S25FL127S;   debug_printf ("S25FL127S\n\r");      break;
    }

    switch (flash)
    {
      case FLASH_TYPE_ADESTO_AT25SF161:
      case FLASH_TYPE_ADESTO_AT25DL161:
        flash_sectors           = 512;
        flash_max_buf           = 256;

        flash_pages_per_sector  = 16;       // 16 x 256 byte pages
        flash_page_size         = 1024 * 4 / flash_pages_per_sector; // 4K sector size
  
        file_system_start_page  = 1 * flash_pages_per_sector; // never start at 0!!!
        file_system_end_page    = (flash_pages_per_sector * flash_sectors) - 1;
        break;
      case FLASH_TYPE_CYPRES_S25FL127S:
        flash_sectors           = 256;
        flash_max_buf           = 256;
  
        flash_pages_per_sector  = 16;        // 16 x 4096 byte pages
        flash_page_size         = 1024 * 64 / flash_pages_per_sector; // 64 K sector size
  
        file_system_start_page  = 1 * flash_pages_per_sector; // never start at 0, use 1st sector for fs info!!!
        file_system_end_page    = (flash_pages_per_sector * flash_sectors) - 1;
        break;
      case FLASH_TYPE_ADESTO_AT25SL128:
        flash_sectors           = 1024 * 4;
        flash_max_buf           = 256;
  
        flash_pages_per_sector  = 16;       // 16 x 256 byte pages
        flash_page_size         = 1024 * 4 / flash_pages_per_sector; // 4K sector size
  
        file_system_start_page  = 1 * flash_pages_per_sector; // never start at 0, use 1st sector for fs info!!!
        file_system_end_page    = (flash_pages_per_sector * flash_sectors) - 1;
        break;
      case FLASH_TYPE_MICRON_MT25QL256:
        flash_sectors           = 1024 * 8;
        flash_max_buf           = 256;
  
        flash_pages_per_sector  = 16;       // 16 x 256 byte pages
        flash_page_size         = 1024 * 4 / flash_pages_per_sector; // 4K sector size
  
        file_system_start_page  = 1 * flash_pages_per_sector; // never start at 0, use 1st sector for fs info!!!
        file_system_end_page    = (flash_pages_per_sector * flash_sectors) - 1;
        break;
      default: Flash_Fail_Callback ();
        return false;
    }

    debug_printf ("flash_max_buf %u\n\r"          , flash_max_buf           );
    debug_printf ("flash_sectors %u\n\r"          , flash_sectors           );
    debug_printf ("flash_sector_size %u\n\r"      , flash_sector_size       );
    debug_printf ("flash_page_size %u\n\r"        , flash_page_size         );
    debug_printf ("flash_pages_per_sector %u\n\r" , flash_pages_per_sector  );
    debug_printf ("file_system_start_page %u\n\r" , file_system_start_page  );
    debug_printf ("file_system_end_page %u\n\r"   , file_system_end_page    );
    
    
#ifndef RELEASE	
#if 0
//    uint8_t buf [256] = "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
    
    int sector = 20, was_sector = 20;
    
    for (int i = flash_sector_size; i < (flash_sectors * flash_sector_size); i += flash_max_buf)
    {
      sector = i / flash_sector_size;
      
      if (sector != was_sector)
      {
        was_sector = sector;
        flash_driver_erase_sector_address (sector, false);
        debug_printf ("Erasing %08d (%05X) of %08d (%05X)\r", i, i, (flash_sectors * flash_sector_size), (flash_sectors * flash_sector_size));
      }
      
      uint8_t buf [256];
      
      for (int j = 0; j < flash_max_buf; j++)
      {
        buf [j] = j % 10;
        buf [j] += '0';
      }
      
//      flash_driver_write (i, buf, sizeof (buf), true);
      
      debug_printf ("testing %08d (%05X) of %08d (%05X)\r", i, i, (flash_sectors * flash_sector_size), (flash_sectors * flash_sector_size));
    }
    
    while (1);
#endif
#endif

  }

  return true;
}

//-------------------------------------------------------------------------------------
#define FLASH_KEY 0x5AA55AA5

static uint32_t write_index = 0;

#define sector_array_size (flash_sector_size / sizeof(uint32_t))

//-------------------------------------------------------------------------------------
typedef struct {
  uint32_t key;
  uint32_t start_page;
  uint32_t size;
  uint32_t crc;
} __attribute__((packed)) flash_info_t;

//-------------------------------------------------------------------------------------
void flash_driver_save_quick_start_fat (uint32_t fat_start_page)
{
  if (flash_sector_size) // initialized
  {
    flash_info_t flash_info = {0};
    
    flash_driver_read (0, (void *)&flash_info, sizeof (flash_info));
    
    if (fat_start_page == 0) // fs wants to reset the start fat page
    {
			debug_printf ("reseting flash info sector\n\r");
      
      flash_driver_erase_sector_address (0, false);
      write_index = 0;
      
      flash_info.key            = FLASH_KEY;
      flash_info.start_page     = 0xFFFFFFFF; // preserve for later writing without erase
      flash_info.size           = 0xFFFFFFFF;
      flash_info.crc            = 0xFFFFFFFF;
      
			flash_driver_write (0, (void *)&flash_info, sizeof(flash_info), true);

      return; // stop here - dont write in zero fat start page
    }
    
    if (flash_info.key != FLASH_KEY) // not initialized
    {
			debug_printf ("flash info sector key fail - reset with fat quick start page\n\r", fat_start_page);
      
      flash_driver_erase_sector_address (0, false);
      write_index = 0;
      
      flash_info.key            = FLASH_KEY;
      flash_info.start_page     = 0xFFFFFFFF; // preserve for later writing without erase
      flash_info.size           = 0xFFFFFFFF;
      flash_info.crc            = 0xFFFFFFFF;
      
			flash_driver_write (0, (void *)&flash_info, sizeof(flash_info), true);
    }
		
    if ((sizeof (flash_info) + (write_index * sizeof (fat_start_page))) > sector_array_size) // out of sector boundry
		{
			debug_printf ("flash info sector index %u out of bounds - reset with fat quick start page %u\n\r", write_index, fat_start_page);
      
      flash_driver_erase_sector_address (0, false);
      write_index = 0;
      
      flash_info.key            = FLASH_KEY;
      flash_info.start_page     = 0xFFFFFFFF; // preserve for later writing without erase
      flash_info.size           = 0xFFFFFFFF;
      flash_info.crc            = 0xFFFFFFFF;
      
			flash_driver_write (0, (void *)&flash_info, sizeof(flash_info), true);
		}

    flash_driver_write (sizeof(flash_info) + (write_index * sizeof (fat_start_page)), (void *)&fat_start_page, sizeof (fat_start_page), true);
    debug_printf ("saving quick start fat page %u at index %u\n\r", fat_start_page, write_index);
    write_index++;
  }
}

//-------------------------------------------------------------------------------------
uint32_t flash_driver_load_quick_start_fat (void)
{
  uint32_t return_page = 0;
  
  if (flash_sector_size)
  {
    flash_info_t flash_info = {0};
    
    flash_driver_read (0, (void *)&flash_info, sizeof (flash_info));
    
    write_index = 0;
    
    if (flash_info.key == FLASH_KEY) // nothing to see here - return zero and leave untill a valid fat page is saved
    {
      while ((sizeof (flash_info) + (write_index * sizeof (uint32_t))) < sector_array_size)
      {
        uint32_t flash_32data;

        flash_driver_read (sizeof (flash_info) + (write_index * sizeof (flash_32data)), (void *)&flash_32data, sizeof (flash_32data));
        
        if (flash_32data != 0xFFFFFFFF)
        {
          write_index++;
          return_page = flash_32data;
        }
        else // return last good read
        {
          break; // from while
        }
      }
    }
  }

  if (return_page)
	{
		debug_printf ("loading quick start fat page %u, index now at %u\n\r", return_page, write_index);
	}
  
  return (return_page);
}

//-------------------------------------------------------------------------------------
