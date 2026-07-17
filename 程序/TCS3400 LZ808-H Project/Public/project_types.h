#ifndef __PROJECT_TYPES_H__
#define __PROJECT_TYPES_H__

#include <STC8H.H>

typedef signed char    int8;
typedef signed int     int16;
typedef signed long    int32;
typedef unsigned char  uint8;
typedef unsigned int   uint16;
typedef unsigned long  uint32;

#define TRUE   1u
#define FALSE  0u

typedef enum
{
    PROJECT_ASYNC_RESULT_NONE = 0,
    PROJECT_ASYNC_RESULT_SUCCESS,
    PROJECT_ASYNC_RESULT_ERROR
} ProjectAsyncResult_t;

#endif
