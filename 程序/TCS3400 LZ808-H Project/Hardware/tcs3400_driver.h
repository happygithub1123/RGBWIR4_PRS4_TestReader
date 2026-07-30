#ifndef __TCS3400_DRIVER_H__
#define __TCS3400_DRIVER_H__

#include "soft_i2c.h"

typedef enum
{
    TCS3400_CHANNEL_CLEAR = 0,
    TCS3400_CHANNEL_RED,
    TCS3400_CHANNEL_GREEN,
    TCS3400_CHANNEL_BLUE
} Tcs3400Channel_t;

bit TCS3400_Init(SoftI2C_Bus_t bus, uint8 atime, uint8 gain); /* 初始化指定传感器。 */
bit TCS3400_SetATIME(SoftI2C_Bus_t bus, uint8 atime);        /* 设置并回读校验积分寄存器。 */
bit TCS3400_SetGain(SoftI2C_Bus_t bus, uint8 gain);          /* 设置并回读校验增益寄存器。 */
bit TCS3400_ReadID(SoftI2C_Bus_t bus, uint8 *id);            /* 读取芯片ID。 */
bit TCS3400_RestartIntegration(SoftI2C_Bus_t bus);           /* 重新启动一帧积分。 */
bit TCS3400_GetDataValid(SoftI2C_Bus_t bus, uint8 *valid);   /* 读取AVALID状态。 */
bit TCS3400_ReadChannel(SoftI2C_Bus_t bus, Tcs3400Channel_t channel,
                        uint16 *value);                       /* 连续读取指定通道低、高字节。 */
bit TCS3400_SetIRMap(SoftI2C_Bus_t bus, bit enabled);        /* 开启或关闭IR映射。 */

#endif
