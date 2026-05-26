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
#include "list.h"
#include <string.h>

void *list_add (list_t **indirect, size_t size, const char *debug)
{
  list_t *list_item = malloc (sizeof(list_t) + size);

  if ( list_item )
  {
    memset ( (void *)list_item, 0, sizeof(list_t) + size);

    list_item->debug = debug;
    list_item->size  = size;

    while ( *indirect ) // list exist
    {
      indirect = &(*indirect)->next; // last next pointer will be null
    }

    *indirect = list_item;
    list_item->position = (size_t)list_item;

    return (void *)&list_item->memory;
  }

  return NULL;
}

void list_remove (list_t **indirect, void *blob, cleanup_callback_t cleanup_callback, const char *debug)
{
  list_t *entry;

  while (NULL != (entry = *indirect))
  {
    if (&entry->memory == blob)
    {
      if (cleanup_callback)
        cleanup_callback (&entry->memory);

      (*indirect) = entry->next;
      free (entry);
      return;
    }

    indirect = &(*indirect)->next;
  }
}

void list_clear (list_t **indirect, cleanup_callback_t cleanup_callback, const char *debug)
{
  list_t *entry;

  while ( (entry = (*indirect)) != NULL )
  {
    if (cleanup_callback)
      cleanup_callback (&entry->memory);

    (*indirect) = entry->next;
    free (entry);
  }
}

void *list_first (list_t *list)
{
  if (list) return (void *)&list->memory;
  return NULL;
}

void *list_last (list_t *list)
{
  while (list)
  {
    if (list->next) list = list->next;
    else return (void *)&list->memory;
  }
  return NULL;
}

void *list_walk (list_t **indirect)
{
  void *ret = NULL;
  if ( *indirect )
  {
    ret = &(*indirect)->memory;
    *indirect = (*indirect)->next;
  }
  return ret;
}

void *list_find (list_t *list, void *item)
{
  while (list)
  {
    if (&list->memory == item) return &list->memory;
    list = list->next;
  }
  return NULL;
}

int list_count (list_t *list)
{
  int count = 0;
  while (list)
  {
    count++;
    list = list->next;
  }
  return count;
}

size_t list_mem (list_t *list)
{
  size_t size = 0;
  while (list)
  {
    size += list->size;
    list = list->next;
  }
  return size;
}

size_t list_mem2 (list_t *list)
{
  size_t size = 0;
  while (list)
  {
    size += sizeof(list_t);
    size += list->size;
    list = list->next;
  }
  return size;
}
