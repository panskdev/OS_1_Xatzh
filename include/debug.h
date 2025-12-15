#ifndef DEBUG_H
#define DEBUG_H

#include <errno.h>
#include <string.h>

#define errorMain 1
#define TEST printf("PASSED %d IN %s\n", __LINE__, __FILE__); if(getchar());

#endif