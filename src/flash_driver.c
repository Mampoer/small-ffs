//-------------------------------------------------------------------------------------
#include "flash_Driver.h"
#include <stdio.h>

//-------------------------------------------------------------------------------------
void DEBUG_printf  ( const char *ptr, ...    );

//-------------------------------------------------------------------------------------
#define debug_printf(x)  DEBUG_printf x 

//-------------------------------------------------------------------------------------
#define     READ_ID                         0x90
#define     WRITE_COMMAND                   0x02
#define     READ_COMMAND                    0x03
#define     WRDI_COMMAND                    0x04
#define     STATUS_COMMAND                  0x05
#define     WREN_COMMAND                    0x06
#define     WRITE_AAI_COMMAND               0xAD
#define     ERASE_COMMAND                   0x20

//-------------------------------------------------------------------------------------
uint16_t    flash_sectors           = 0;
uint16_t    flash_sector_size       = 0;
uint16_t    flash_page_size         = 0;
uint16_t    flash_pages_per_sector  = 0;
uint16_t    flash_first_page        = 0;
uint16_t    flash_last_page         = 0;

//-------------------------------------------------------------------------------------
int         wr_payload_index        = 0;
uint8_t     wr_payload[2]              ;
  
//-------------------------------------------------------------------------------------
// Flash Functions
//-------------------------------------------------------------------------------------
void setupFlashAddress (uint8_t *address, uint16_t sector_address, uint16_t byte_address)
{
  if ( ( byte_address < flash_sector_size ) && ( sector_address < flash_sectors ) )
  {
    union 
    {
      uint32_t addr32;
      uint8_t  addr8[4];
    } addr;
    
    // byte pos
    addr.addr32 = (sector_address * flash_sector_size)  + byte_address;
    
    address[1] = addr.addr8[2]; // little endian
    address[2] = addr.addr8[1];
    address[3] = addr.addr8[0];
  }
}

//-------------------------------------------------------------------------------------
uint8_t sendFlashCommand(uint8_t command,uint8_t *address_pointer)
{
  uint8_t status = 1;

  Select_Flash();

  if (command == STATUS_COMMAND)
  {
    spi_send_byte(command);
    status = spi_read_byte();
    Deselect_Flash();
  }
  else
  {
    *address_pointer = command;
    Write_Data_Page(address_pointer,4);
    switch    (command)
    {
////      case (READ_MAIN_MEMORY_COMMAND):      // 0x52 or 0xD2
////        spi_send_byte(0);                   // SEND ONE DUMMY uint8_t
////        spi_send_byte(0);                   // SEND ONE DUMMY uint8_t
////        spi_send_byte(0);                   // SEND ONE DUMMY uint8_t
////        spi_send_byte(0);                   // SEND ONE DUMMY uint8_t
////        break;
////      case (BUFFER1_READ_COMMAND):          // 0x54 or 0xD4
////      case (BUFFER2_READ_COMMAND):          // 0x56 or 0xD6
////        spi_send_byte(0);                   // SEND ONE DUMMY uint8_t
////        break;
      case (READ_ID):                       // 0x90 DONT WANT TO DESELECT
      case (READ_COMMAND):                  // 0x03 DONT WANT TO DESELECT
      case (WRITE_COMMAND):                 // 0x02 DONT WANT TO DESELECT
      case (WRITE_AAI_COMMAND):             // 0xAD DONT WANT TO DESELECT
        break;
      case (ERASE_COMMAND):                 // 0x20
        Deselect_Flash();
        break;
      default:
        break;
    }
  }
    
  return status;
}

//-------------------------------------------------------------------------------------
uint8_t flashBusyDelay(void)
{
  uint8_t status;

  do
  {
    status = sendFlashCommand(STATUS_COMMAND,NULL);
    
    if ((status & 0x01) == 0x01)
      if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING ) taskYIELD();
    
  } while((status & 0x01) == 0x01);
  
  return status;
}

//-------------------------------------------------------------------------------------
void Flash_WREN(void)
{
  flashBusyDelay();    // DELAY UNTIL NOT BUZY
  Select_Flash();
  spi_send_byte(WREN_COMMAND);
  Deselect_Flash();
}

//-------------------------------------------------------------------------------------
void Flash_WRDI(void)
{
  flashBusyDelay();    // DELAY UNTIL NOT BUZY
  Select_Flash();
  spi_send_byte(WRDI_COMMAND);
  Deselect_Flash();
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void Flash_Erase_Sector(uint16_t sector_num)
{
  uint8_t   address[4];

  Flash_WREN();
  
  //debug_printf (("Flash_Erase_Sector @ %hu\n\r", sector_num));
  setupFlashAddress(address, sector_num, 0);
  (void)flashBusyDelay();    // DELAY UNTIL NOT BUZY
  (void)sendFlashCommand(ERASE_COMMAND,address); // READ THE FLASH DIRECTLY
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void flash_driver_set_flash_read(uint16_t sector_address, uint16_t byte_address)
{
  uint8_t address[4];
  setupFlashAddress(address, sector_address, byte_address);
  (void)flashBusyDelay();    // DELAY UNTIL NOT BUZY
  (void)sendFlashCommand(READ_COMMAND,address); // READ THE FLASH DIRECTLY
}

//-------------------------------------------------------------------------------------
void flash_driver_read(uint8_t *data, uint16_t len)
{
  Read_Data_Page(data, len);
}

//-------------------------------------------------------------------------------------
void flash_driver_read_release(void)
{
  Deselect_Flash();
}

//-------------------------------------------------------------------------------------
void flash_driver_set_flash_write(uint16_t sector_num, uint16_t byte_address)
{
  uint8_t address[4];

//  Select_Flash();
//  spi_send_byte(0x80);
//  Deselect_Flash();
  
  Flash_WREN();
  
  //debug_printf(("Write_Sector_Info @ %hu\n\r", sector_num));
  setupFlashAddress(address, sector_num, byte_address & 0xFFFE); // only even addr allowed
  (void)flashBusyDelay();    // DELAY UNTIL NOT BUZY
  (void)sendFlashCommand(WRITE_AAI_COMMAND,address); // Write THE FLASH DIRECTLY
  
  wr_payload_index = 0;
  
  if (byte_address & 0x0001) // odd start addr - back pad
  {
    wr_payload[wr_payload_index++] = 0xFF;
  }
}

//-------------------------------------------------------------------------------------
void flash_driver_write(const uint8_t *data, uint16_t len)
{
  while ( len )
  {
    wr_payload[wr_payload_index++] = *data++;
    len--;

    if ( wr_payload_index == 2 )
    {
      Write_Data_Page(wr_payload,2);
      Deselect_Flash();
      wr_payload_index = 0;
      
      if ( len ) // still someting left
      {
        (void)flashBusyDelay();    // DELAY UNTIL NOT BUZY
        Select_Flash();
        spi_send_byte(WRITE_AAI_COMMAND);
      }
    }
  }
}

//-------------------------------------------------------------------------------------
void flash_driver_write_release(void)
{
  if ( wr_payload_index )
  {
    wr_payload[wr_payload_index++] = 0xFF;
    Write_Data_Page(wr_payload,2);
    Deselect_Flash();
    wr_payload_index = 0;
  }
    
  Flash_WRDI();
}

//-------------------------------------------------------------------------------------
void Read_Flash_ID(uint8_t *id)
{
  uint8_t address[4];
  setupFlashAddress(address, 0, 0);
  (void)flashBusyDelay();    // DELAY UNTIL NOT BUZY
  (void)sendFlashCommand(READ_ID,address);
  Read_Data_Page(id,2);
  Deselect_Flash();
}

//-------------------------------------------------------------------------------------
bool init_flash(void)
{
  uint8_t status;

  uint8_t id[2];

  init_flash_hw();

  status = flashBusyDelay();

  if ( status & 0x40 )
  {
    Flash_WRDI();
  }

  Read_Flash_ID(id);
  
  switch(id[0])
  {
    case 0xBF:    // Microchp
      switch (id[1])
      {
        case 0x8E:
          debug_printf(("1 MEG\n\r"));
          flash_sectors           =  256;
          flash_sector_size       = 4096;
        
          flash_pages_per_sector  = 8;
          flash_page_size         = (flash_sector_size - sector_info_size) / flash_pages_per_sector;
        
          flash_first_page        = 1; // never start at 0!!!
          flash_last_page         = 2047;
          break;
        default:
          debug_printf(("Unknown Flash device\n\r"));
          Flash_Fail_Callback();
          return false;
      }
      break;
    default:
      debug_printf(("Unknown Flash device\n\r"));
      Flash_Fail_Callback();
      return false;
  }
  
  debug_printf(("flash_sectors %u\n\r",flash_sectors));
  debug_printf(("flash_sector_size %u\n\r",flash_sector_size));
  debug_printf(("flash_page_size %u\n\r",flash_page_size));
  debug_printf(("flash_pages_per_sector %u\n\r",flash_pages_per_sector));
  debug_printf(("flash_first_page %u\n\r",flash_first_page));
  debug_printf(("flash_last_page %u\n\r",flash_last_page));

  return true;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
