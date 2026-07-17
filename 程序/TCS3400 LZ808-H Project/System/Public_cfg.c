#include "Public_cfg.h"

/* 毫秒延时函数，用于上电初始化时序 */
void Delay_ms(unsigned int xms)
{
    unsigned int i, j;
    for (i = 0; i < xms; i++)
        for (j = 0; j < 2760; j++);  /* 内部22.1184MHz粗略延时1ms */
}

/* 外部晶振选择函数（当前未使用） */
void XtalSelect_Init(void)
{
    // 预留外部晶振选择代码
}
