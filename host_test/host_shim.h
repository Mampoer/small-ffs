//-------------------------------------------------------------------------------------
// Host build shim. Force-included into EVERY translation unit via cl.exe /FI.
//
//   * Always-on: silence GCC __attribute__ (MSVC doesn't understand it).
//   * Always-on: drop the Keil ZERO_INIT section attribute.
//   * Only when FS_HOST_FS_C is defined (i.e. compiling the shadowed fs.c):
//     - provide `FILE` as `struct __FILE`
//     - remap stdio names to fs_-prefixed
//     - undef FILE_PRINTF
//-------------------------------------------------------------------------------------
#ifndef _SMALL_FFS_HOST_SHIM_H_
#define _SMALL_FFS_HOST_SHIM_H_

#ifndef __GNUC__
#define __attribute__(x)
#endif

#ifdef FS_HOST_FS_C

  struct __FILE;
  #define FILE struct __FILE

  // stdio constants — fs.c uses these but we stripped <stdio.h>.
  #define EOF       (-1)
  #define SEEK_SET  0
  #define SEEK_CUR  1
  #define SEEK_END  2

  #define fopen   fs_fopen
  #define fclose  fs_fclose
  #define fputc   fs_fputc
  #define fgetc   fs_fgetc
  #define fputs   fs_fputs
  #define fgets   fs_fgets
  #define fread   fs_fread
  #define fwrite  fs_fwrite
  #define fseek   fs_fseek
  #define remove  fs_remove
  #define rename  fs_rename

  #ifdef FILE_PRINTF
  #undef FILE_PRINTF
  #endif

#endif

#endif
