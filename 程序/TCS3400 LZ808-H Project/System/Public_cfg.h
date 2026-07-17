#ifndef __PUBLIC_CFG_H__
#define __PUBLIC_CFG_H__

#include "stc8h.h"

/* 2. 公共变量类型定义 */
typedef unsigned long  uint32;
typedef unsigned int   uint16;
typedef unsigned char  uint8;

/* 3. 通用宏定义 */
#define TRUE        1
#define FALSE       0

/* 4. 设备/引脚状态定义 */
#define TURN_ON     0
#define TURN_OFF    1

/* 5. 系统相关宏（可选） */
//#define FOSC      22118400UL

/* 6. 外部函数声明 */
void Delay_ms(unsigned int xms);      // @22.1184MHz
void XtalSelect_Init(void);       // 外部时钟选择


#endif