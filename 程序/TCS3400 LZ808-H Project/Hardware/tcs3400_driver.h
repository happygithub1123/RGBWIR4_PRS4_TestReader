#ifndef __TCS3400_DRIVER_H__
#define __TCS3400_DRIVER_H__ // 防止本项目TCS3400驱动接口被重复包含

#include "soft_i2c.h"

typedef enum
{
    TCS3400_CHANNEL_CLEAR = 0, // 本项目中选择Clear低、高字节寄存器
    TCS3400_CHANNEL_RED,       // 本项目中选择R低、高字节寄存器
    TCS3400_CHANNEL_GREEN,     // 本项目中选择G低、高字节寄存器
    TCS3400_CHANNEL_BLUE       // 本项目中选择B低、高字节寄存器
} Tcs3400Channel_t; // 本项目中指定普通环境光命令实际需要读取的一个通道

bit TCS3400_Init(SoftI2C_Bus_t bus, uint8 atime, uint8 gain); // 初始化本项目指定路TCS3400
bit TCS3400_SetATIME(SoftI2C_Bus_t bus, uint8 atime); // 设置并回读本项目积分寄存器
bit TCS3400_SetGain(SoftI2C_Bus_t bus, uint8 gain); // 设置并回读本项目增益寄存器
bit TCS3400_ReadID(SoftI2C_Bus_t bus, uint8 *id); // 读取本项目指定路芯片ID
bit TCS3400_RestartIntegration(SoftI2C_Bus_t bus); // 重新启动本项目一帧环境光积分
bit TCS3400_GetDataValid(SoftI2C_Bus_t bus, uint8 *valid); // 读取本项目AVALID状态
bit TCS3400_ReadChannel(SoftI2C_Bus_t bus, Tcs3400Channel_t channel, uint16 *value); // 只读取本项目当前命令指定的普通通道
bit TCS3400_ReadClearPort(SoftI2C_Bus_t bus, uint16 *value); // 读取本项目Clear或IR映射值
bit TCS3400_SetIRMap(SoftI2C_Bus_t bus, bit enabled); // 开启或关闭本项目IR映射

#endif
