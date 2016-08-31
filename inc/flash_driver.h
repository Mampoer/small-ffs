/*-----------------------------------------------------------------------------------*/
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
/*-----------------------------------------------------------------------------------*/
#ifndef __FLASH_DRIVER_H__
#define __FLASH_DRIVER_H__

//-------------------------------------------------------------------------------------
#include "flash_config.h"

//-------------------------------------------------------------------------------------
#define WEAR_THRESHOLD        5000
#define MAX_WEAR_THRESHOLD    90000

//-------------------------------------------------------------------------------------
typedef union sector_info
{
  uint32_t      erase_count;
  struct
  {
    uint8_t       pad[3];
    uint8_t       flags;
  } __attribute__((packed)) bytes;
} __attribute__((packed)) sector_info_t;

//-------------------------------------------------------------------------------------
//extern uint16_t    flash_sectors           ;
//extern uint16_t    flash_sector_size       ;
extern uint16_t    flash_page_size         ;
extern uint16_t    flash_pages_per_sector  ;
extern uint16_t    flash_first_page        ;
extern uint16_t    flash_last_page         ;

//-------------------------------------------------------------------------------------
#define sector_info_size          sizeof(sector_info_t)
#define sector_number(page)       (page / flash_pages_per_sector)

//-------------------------------------------------------------------------------------
bool init_flash(void);

//-------------------------------------------------------------------------------------
void flash_driver_set_flash_read(uint16_t sector_address, uint16_t byte_address);
void flash_driver_set_flash_write(uint16_t sector_address, uint16_t byte_address);
void flash_driver_read(uint8_t *data, uint16_t len);
void flash_driver_write(const uint8_t *data, uint16_t len);
void flash_driver_read_release(void);
void flash_driver_write_release(void);

//-------------------------------------------------------------------------------------
void Flash_Erase_Sector(uint16_t sector_num);

//-------------------------------------------------------------------------------------
void      flash_sector_protect_callback(uint8_t status);
void      Flash_Fail_Callback(void);

//void      Format_Flash_Callback(void);

void      watchdog_reset_callback(void);

//-------------------------------------------------------------------------------------
#endif
//-------------------------------------------------------------------------------------
