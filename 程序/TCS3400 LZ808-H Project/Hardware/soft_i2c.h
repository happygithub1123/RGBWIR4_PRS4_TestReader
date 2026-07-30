#ifndef __SOFT_I2C_H__
#define __SOFT_I2C_H__ // 防止本项目四路软件I2C接口被重复包含

#include "Public_cfg.h"

typedef enum
{
    SOFT_I2C_BUS_1 = 0, // 本项目中选择P0.4为SCL、P5.2为SDA的第1路TCS3400
    SOFT_I2C_BUS_2,     // 本项目中选择P0.1为SCL、P0.2为SDA的第2路TCS3400
    SOFT_I2C_BUS_3,     // 本项目中选择P4.5为SCL、P4.6为SDA的第3路TCS3400
    SOFT_I2C_BUS_4,     // 本项目中选择P2.5为SCL、P2.6为SDA的第4路TCS3400
    SOFT_I2C_BUS_COUNT  // 本项目中表示软件I2C总线总数，用于参数范围判断
} SoftI2C_Bus_t; // 本项目中用于把四路TCS3400编号映射到各自独立的SCL和SDA引脚

void SoftI2C_Init(void); // 初始化并释放本项目四路TCS3400软件I2C总线
bit SoftI2C_WriteRegister(SoftI2C_Bus_t bus, uint8 address, uint8 reg, uint8 value); // 向指定路TCS3400写单寄存器
bit SoftI2C_ReadRegister(SoftI2C_Bus_t bus, uint8 address, uint8 reg, uint8 *value); // 从指定路TCS3400读单寄存器
bit SoftI2C_ReadRegisters(SoftI2C_Bus_t bus, uint8 address, uint8 reg,
                          uint8 *buffer, uint8 length); // 从指定路TCS3400连续读取寄存器

#endif
