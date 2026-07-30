#ifndef __PROJECT_TYPES_H__
#define __PROJECT_TYPES_H__ // 防止本项目公共数据类型被重复定义

#include <STC8H.H>

typedef signed char    int8;
typedef signed int     int16;
typedef signed long    int32;
typedef unsigned char  uint8;
typedef unsigned int   uint16;
typedef unsigned long  uint32;

typedef enum
{
    PROJECT_ASYNC_RESULT_NONE = 0, // 本项目中表示环境光或压力异步命令仍在执行或结果已被取走
    PROJECT_ASYNC_RESULT_SUCCESS,  // 本项目中表示异步命令成功，Float结果可以返回ICT
    PROJECT_ASYNC_RESULT_ERROR     // 本项目中表示异步命令失败，应用层应向ICT返回-1.0
} ProjectAsyncResult_t; // 本项目中用于统一表示环境光与压力控制器的异步执行结果

#endif
