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
#ifndef __LIST_H__
#define __LIST_H__

#include <stdlib.h>

typedef void (*cleanup_callback_t)(void *);

typedef struct list {
    struct list *next;
    size_t      position;
    size_t      size;
    const char  *debug;
    char        memory;
} list_t;

void    *list_add    (list_t **indirect, size_t size, const char *debug);
void    *list_first  (list_t *list);
void    *list_last   (list_t *list);
int     list_count   (list_t *list);
size_t  list_mem     (list_t *list);
size_t  list_mem2    (list_t *list);
void    *list_walk   (list_t **indirect);
void    *list_find   (list_t *list, void *item);
void    list_clear   (list_t **indirect, cleanup_callback_t cleanup_callback, const char *debug);
void    list_remove  (list_t **indirect, void *blob, cleanup_callback_t cleanup_callback, const char *debug);

#endif
