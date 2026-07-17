#ifndef __TCS3400_DRIVER_H__
#define __TCS3400_DRIVER_H__

#include "i2c_bus.h"

/**
 * @file tcs3400_driver.h
 * @brief TCS3400 颜色传感器驱动层对外接口定义
 *
 * @details
 * 定义 TCS3400 的 I2C 地址、寄存器地址、控制位、增益参数，
 * 以及初始化、参数配置和原始数据读取接口。
 */

/*==============================================================================
 * I2C 地址定义
 *============================================================================*/

#define TCS3400_I2C_ADDR_7BIT  0x39                                /**< TCS3400 的 7 位 I2C 地址 */
#define TCS3400_I2C_ADDR_W     (TCS3400_I2C_ADDR_7BIT << 1)         /**< TCS3400 的 8 位写地址 */
#define TCS3400_I2C_ADDR_R     ((TCS3400_I2C_ADDR_7BIT << 1) | 1)   /**< TCS3400 的 8 位读地址 */

/*==============================================================================
 * 寄存器地址定义
 *============================================================================*/

#define TCS3400_ENABLE         0x80  /**< 使能控制寄存器 */
#define TCS3400_ATIME          0x81  /**< RGBC ADC 积分时间寄存器 */
#define TCS3400_WTIME          0x83  /**< 等待时间寄存器 */
#define TCS3400_CONFIG         0x8D  /**< 配置寄存器 */
#define TCS3400_CONTROL        0x8F  /**< 模拟增益控制寄存器 */
#define TCS3400_REVID          0x91  /**< 芯片版本号寄存器 */
#define TCS3400_ID             0x92  /**< 芯片 ID 寄存器 */
#define TCS3400_STATUS         0x93  /**< 设备状态寄存器 */

#define TCS3400_CDATAL         0x94  /**< Clear 通道数据低字节寄存器 */
#define TCS3400_CDATAH         0x95  /**< Clear 通道数据高字节寄存器 */
#define TCS3400_RDATAL         0x96  /**< Red 通道数据低字节寄存器 */
#define TCS3400_RDATAH         0x97  /**< Red 通道数据高字节寄存器 */
#define TCS3400_GDATAL         0x98  /**< Green 通道数据低字节寄存器 */
#define TCS3400_GDATAH         0x99  /**< Green 通道数据高字节寄存器 */
#define TCS3400_BDATAL         0x9A  /**< Blue 通道数据低字节寄存器 */
#define TCS3400_BDATAH         0x9B  /**< Blue 通道数据高字节寄存器 */

#define TCS3400_IR             0xC0  /**< IR 映射控制寄存器 */

/*==============================================================================
 * ENABLE 寄存器控制位
 *============================================================================*/

#define TCS3400_EN_PON         0x01  /**< 芯片上电使能位 */
#define TCS3400_EN_AEN         0x02  /**< RGBC ADC 采样使能位 */
#define TCS3400_EN_WEN         0x08  /**< 等待功能使能位 */

/*==============================================================================
 * 增益参数定义
 *============================================================================*/

#define TCS3400_GAIN_1X        0x00  /**< 1 倍模拟增益 */
#define TCS3400_GAIN_4X        0x01  /**< 4 倍模拟增益 */
#define TCS3400_GAIN_16X       0x02  /**< 16 倍模拟增益 */
#define TCS3400_GAIN_64X       0x03  /**< 64 倍模拟增益 */

#define TCS3400_DEFAULT_GAIN   TCS3400_GAIN_16X  /**< 默认使用 16 倍模拟增益 */

/*==============================================================================
 * 配置和状态位定义
 *============================================================================*/

#define TCS3400_CFG_WLONG      0x02  /**< 等待时间延长控制位 */
#define TCS3400_STATUS_AVALID  0x01  /**< RGBC 数据有效状态位 */
#define TCS3400_STATUS_ASAT    0x80  /**< 模拟通道饱和状态位 */

/*==============================================================================
 * IR 映射参数定义
 *============================================================================*/

#define TCS3400_IR_ENABLE      0x80  /**< 开启 Clear 数据端口的 IR 映射 */
#define TCS3400_IR_DISABLE     0x00  /**< 关闭 Clear 数据端口的 IR 映射 */

/*==============================================================================
 * 对外开放函数声明
 *============================================================================*/

bit TCS3400_Init(I2C_Bus_t bus);                                                          /**< 初始化指定总线上的 TCS3400 */
uint8 TCS3400_CalcATIME(uint16 time_ms);                                                   /**< 将积分时间换算为 ATIME 寄存器值 */
bit TCS3400_SetIntegrationTime(I2C_Bus_t bus, uint16 time_ms);                             /**< 设置积分时间并更新帧等待时间 */
uint16 TCS3400_GetFrameWaitMs(I2C_Bus_t bus);                                              /**< 获取当前配置对应的帧等待时间 */
bit TCS3400_SetATIME(I2C_Bus_t bus, uint8 atime);                                          /**< 直接设置 ATIME 寄存器 */
bit TCS3400_SetGain(I2C_Bus_t bus, uint8 gain);                                            /**< 设置传感器模拟增益 */
bit TCS3400_SetWTIME(I2C_Bus_t bus, uint8 wtime);                                          /**< 设置等待时间寄存器 */
bit TCS3400_ReadID(I2C_Bus_t bus, uint8 *id);                                              /**< 读取传感器 ID */
bit TCS3400_ReadRegValue(I2C_Bus_t bus, uint8 reg, uint8 *value);                          /**< 读取指定的 8 位寄存器 */
bit TCS3400_GetDataValid(I2C_Bus_t bus, uint8 *valid);                                     /**< 读取 STATUS.AVALID 状态 */
bit TCS3400_RestartIntegration(I2C_Bus_t bus);                                             /**< 关闭并重新开启 AEN 以重启积分 */
bit TCS3400_ReadRGBC_NoWait(I2C_Bus_t bus, uint16 *r, uint16 *g, uint16 *b, uint16 *c);    /**< 不等待 AVALID，直接读取 RGBC */
bit TCS3400_ReadClearPort_NoWait(I2C_Bus_t bus, uint16 *value);                            /**< 不等待 AVALID，读取 Clear 数据端口 */
bit TCS3400_ReadRGBC(I2C_Bus_t bus, uint16 *r, uint16 *g, uint16 *b, uint16 *c);           /**< 等待新数据后读取 RGBC */
bit TCS3400_SetIRMapEnable(I2C_Bus_t bus, bit en);                                         /**< 设置 Clear 数据端口的 IR 映射状态 */
uint16 TCS3400_ReadIR(I2C_Bus_t bus);                                                      /**< 读取 IR 原始计数 */
bit TCS3400_ReadRGBIR(I2C_Bus_t bus, uint16 *r, uint16 *g, uint16 *b, uint16 *ir);         /**< 读取 IR 映射状态下的 RGB 和 IR */
uint16 TCS3400_ReadR(I2C_Bus_t bus);                                                       /**< 读取 Red 通道原始计数 */
uint16 TCS3400_ReadG(I2C_Bus_t bus);                                                       /**< 读取 Green 通道原始计数 */
uint16 TCS3400_ReadB(I2C_Bus_t bus);                                                       /**< 读取 Blue 通道原始计数 */
uint16 TCS3400_ReadC(I2C_Bus_t bus);                                                       /**< 读取 Clear 通道原始计数 */
uint16 TCS3400_ReadIR_Only(I2C_Bus_t bus);                                                 /**< 仅读取 IR 通道原始计数 */
bit TCS3400_ReadRaw(I2C_Bus_t bus, uint16 *r, uint16 *g, uint16 *b, uint16 *c, uint16 *ir); /**< 读取 RGBC 和 IR 原始数据 */

#endif /* __TCS3400_DRIVER_H__ */

