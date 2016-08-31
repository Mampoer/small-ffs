/*----------------------------------------------------------------------------
 * Name:    Retarget.c
 * Purpose: 'Retarget' layer for target-dependent low level functions
 * Note(s):
 *----------------------------------------------------------------------------
 * This file is part of the uVision/ARM development tools.
 * This software may only be used under the terms of a valid, current,
 * end user licence from KEIL for a compatible version of KEIL software
 * development tools. Nothing else gives you the right to use this software.
 *
 * This software is supplied "AS IS" without warranties of any kind.
 *
 * Copyright (c) 2011 Keil - An ARM Company. All rights reserved.
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdarg.h>

#include "application.h"

#pragma import(__use_no_semihosting_swi)

int DEBUG_putchar(int c);

int uart_putchar( USART_TypeDef *Instance, int c );

int (*putchar_func_pointer)(int) = DEBUG_putchar;

struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;
FILE __stdin;


int fputc(int c, FILE *f) {
  if ( putchar_func_pointer ) return(putchar_func_pointer(c));
  return (c);
}


int fgetc(FILE *f) {
  return 0;
}


int ferror(FILE *f) {
  /* Your implementation of ferror */
  return EOF;
}


void _ttywrch(int c) {
  if ( putchar_func_pointer ) putchar_func_pointer(c);
}


void _sys_exit(int return_code) {
label:  goto label;  /* endless loop */
}


int DEBUG_putchar(int c)
{
//#ifdef CoreDebug
//  if (CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)
//  {
//    while ( ITM[0].PORT->u32 == 0);
//    ITM[0].PORT->u8 = c;
//  }
//#else
 #ifdef RS485_DEBUG
  
    uart_putchar( USART1, c );
  
    if ( c == '\n' )
      uart_putchar( USART1, '\r' );
      
 #endif
//#endif
  
  return c;
}

void DEBUG_printf( const char *ptr, ... )
{
//  #ifdef APP
    int (*putchar_ptr_save)(int) = putchar_func_pointer;
    va_list var_ptr;
    RS485_WRITE_DIRECTION();
    va_start(var_ptr,ptr);
    putchar_func_pointer = DEBUG_putchar;
    (void)vprintf ((char *)ptr, var_ptr);
    va_end(var_ptr);
    putchar_func_pointer = putchar_ptr_save;
//  #endif
}
