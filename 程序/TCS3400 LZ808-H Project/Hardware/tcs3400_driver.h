#ifndef __TCS3400_DRIVER_H__
#define __TCS3400_DRIVER_H__

#include "soft_i2c.h"

typedef struct
{
    uint16 clear;
    uint16 red;
    uint16 green;
    uint16 blue;
} Tcs3400RgbcData_t;

bit TCS3400_Init(SoftI2C_Bus_t bus, uint8 atime, uint8 gain); /* 初始化指定传感器。 */
bit TCS3400_SetATIME(SoftI2C_Bus_t bus, uint8 atime);        /* 设置并回读校验积分寄存器。 */
bit TCS3400_SetGain(SoftI2C_Bus_t bus, uint8 gain);          /* 设置并回读校验增益寄存器。 */
bit TCS3400_ReadID(SoftI2C_Bus_t bus, uint8 *id);            /* 读取芯片ID。 */
bit TCS3400_RestartIntegration(SoftI2C_Bus_t bus);           /* 重新启动一帧积分。 */
bit TCS3400_GetDataValid(SoftI2C_Bus_t bus, uint8 *valid);   /* 读取AVALID状态。 */
bit TCS3400_ReadRGBC(SoftI2C_Bus_t bus, Tcs3400RgbcData_t *rgbc); /* 读取RGBC原始值。 */
bit TCS3400_ReadClearPort(SoftI2C_Bus_t bus, uint16 *value); /* 读取Clear或IR映射值。 */
bit TCS3400_SetIRMap(SoftI2C_Bus_t bus, bit enabled);        /* 开启或关闭IR映射。 */

#endif
