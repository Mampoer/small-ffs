#ifndef __FLASH_HW_H__
#define __FLASH_HW_H__

#include <stdint.h>
#include <stdbool.h>

/*
*      THIS FILE 'fs_hw.h' IS THE FILE SYSTEM HARDWARE FILE.
*      IN THIS FILE ARE THE DEFINES FOR THE HARDWARE PLATFORM WHERE THE
*      FILE SYSTEM IS RUNNING ON. THE DEFINES ARE FOR THE HARDWARE INTERFACE
*      TO THE FLASH CHIP.
*/

//#define     LINK_SIZE                       16

void        init_flash_hw         (void);

void        Select_Flash          (void);
void        Deselect_Flash        (void);
void        Read_Data_Page        (uint8_t *received_data, uint16_t len);
void        Write_Data_Page       (const uint8_t *transmit_data, uint32_t len);
//void    		Write_Blank_Data_Page (uint16 len);

void        spi_send_byte         (uint8_t dat);
uint8_t     spi_read_byte         (void);

void        Flash_WP              (bool wp);
void        Flash_Hold            (bool wp);

#endif /*  __FS_HW_H */
