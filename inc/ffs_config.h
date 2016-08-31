#ifndef __FFS_CONFIG_H__
#define __FFS_CONFIG_H__

#include "flash_driver.h" 

#include <ctype.h>

#define fs_isprint(x)         isprint(x)
#define fs_toupper(x)         toupper(x)

#define MAX_OPEN_FILES                4
#define MAX_FNAME_SIZE               32      // null terminated string = 1 less than declared

#define poll_callback              poll

#define DEBUG_LOG
#define FILE_PRINTF

#endif
