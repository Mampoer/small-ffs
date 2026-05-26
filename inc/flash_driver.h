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
#ifndef __FLASH_DRIVER_H__
#define __FLASH_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

#include "flash_hw.h"

//-------------------------------------------------------------------------------------
// Wear leveling thresholds. WEAR_THRESHOLD is the initial gap below the global max
// erase-count under which a sector is still considered "fair game" for new FAT pages.
// The threshold is bumped by WEAR_THRESHOLD whenever no sector is found below it,
// capped at MAX_WEAR_THRESHOLD.
//-------------------------------------------------------------------------------------
#define WEAR_THRESHOLD        5000
#define MAX_WEAR_THRESHOLD    90000

//-------------------------------------------------------------------------------------
// sector_info_t lives at the first bytes of every sector. The 24-bit erase_count is
// incremented each time the sector is erased; the high byte (flags) holds
// ERASE_COUNT_KEY when valid so a freshly erased / virgin sector is distinguishable.
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
// Runtime geometry. flash_driver_init() detects the chip and fills these in. The
// file system uses [file_system_start_page, file_system_end_page) — pages outside
// that range may be reserved for the boot loader / quick-start FAT cache.
//-------------------------------------------------------------------------------------
extern uint32_t    flash_page_size         ;
extern uint32_t    flash_pages_per_sector  ;
extern uint32_t    file_system_start_page  ;
extern uint32_t    file_system_end_page    ;

//-------------------------------------------------------------------------------------
#define flash_sector_size (flash_pages_per_sector * flash_page_size)

//-------------------------------------------------------------------------------------
#define sector_info_size sizeof(sector_info_t)

//-------------------------------------------------------------------------------------
bool      flash_driver_init                     (void);
void      hw_init_flash                         (void);

//-------------------------------------------------------------------------------------
void      flash_driver_read                     (uint32_t flash_address, uint8_t *data, uint32_t len);
int       flash_driver_write                    (uint32_t flash_address, const uint8_t *data, uint32_t len, bool verify);

//-------------------------------------------------------------------------------------
void      flash_driver_erase_sector_address     (uint32_t flash_address, bool erase64k);

//-------------------------------------------------------------------------------------
void      flash_sector_protect_callback         (uint8_t status);
void      Flash_Fail_Callback                   (void);

void      flash_driver_blocking_callback        (void);
void      flash_driver_blocking_callback_start  (void);
void      flash_driver_blocking_callback_end    (void);

//-------------------------------------------------------------------------------------
#endif
//-------------------------------------------------------------------------------------
