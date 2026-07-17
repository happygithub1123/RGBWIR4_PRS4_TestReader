#ifndef __SOFT_I2C_H__
#define __SOFT_I2C_H__

#include "Public_cfg.h"

typedef enum
{
    SOFT_I2C_BUS_1 = 0,
    SOFT_I2C_BUS_2,
    SOFT_I2C_BUS_3,
    SOFT_I2C_BUS_4,
    SOFT_I2C_BUS_COUNT
} SoftI2C_Bus_t;

void SoftI2C_Init(void);  /* 初始化并释放四路软件I2C总线。 */
bit SoftI2C_WriteRegister(SoftI2C_Bus_t bus, uint8 address, uint8 reg, uint8 value); /* 写单寄存器。 */
bit SoftI2C_ReadRegister(SoftI2C_Bus_t bus, uint8 address, uint8 reg, uint8 *value); /* 读单寄存器。 */
bit SoftI2C_ReadRegisters(SoftI2C_Bus_t bus, uint8 address, uint8 reg,
                          uint8 *buffer, uint8 length); /* 连续读取寄存器。 */

#endif
