#ifndef __ACTION_CONTROL_H__
#define __ACTION_CONTROL_H__

#include "Public_cfg.h"

/* X输入通道 */
#define X1                 1
#define X2                 2
#define X3                 3
#define X4                 4

/* Y通道定义 */
#define X_DETECTED         0
#define X_NOT_DETECTED     1

/* Y输出通道 */
#define Y1                 1
#define Y2                 2
#define Y3                 3
#define Y4                 4

/* Y引脚状态定义 */
#define Y_ON               1
#define Y_OFF              0

void Yn_Init(void);
void Yn_Control(unsigned char yn, unsigned char state);
void Y_AllOn(void);
void Y_AllOff(void);

unsigned char Xn_Read(unsigned char xn);
void XY_Control(void);

void Run_LED_Blink(unsigned int xms);

#endif