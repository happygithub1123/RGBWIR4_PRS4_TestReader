#include "tcs3400_driver.h"
#include "Delay.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define TCS3400_I2C_ADDRESS       0x39u
#define TCS3400_EXPECTED_ID       0x90u
#define TCS3400_DEVICE_ID_MASK    0xFCu

#define TCS3400_REG_ENABLE        0x80u
#define TCS3400_REG_ATIME         0x81u
#define TCS3400_REG_CONFIG        0x8Du
#define TCS3400_REG_CONTROL       0x8Fu
#define TCS3400_REG_ID            0x92u
#define TCS3400_REG_STATUS        0x93u
#define TCS3400_REG_CLEAR_LOW     0x94u
#define TCS3400_REG_RED_LOW       0x96u
#define TCS3400_REG_GREEN_LOW     0x98u
#define TCS3400_REG_BLUE_LOW      0x9Au
#define TCS3400_REG_IR_MAP        0xC0u

#define TCS3400_ENABLE_PON        0x01u
#define TCS3400_ENABLE_AEN        0x02u
#define TCS3400_STATUS_AVALID     0x01u
#define TCS3400_IR_MAP_BIT        0x80u
#define TCS3400_CONFIG_REQUIRED   0x40u

/**
 * @brief  读取16位小端寄存器值
 * @param  bus 总线编号
 * @param  reg_low 低字节寄存器地址
 * @param  value 读取值存放地址
 * @retval 1：读取成功；0：参数或通信错误
 */
static bit TCS3400_ReadUint16(SoftI2C_Bus_t bus, uint8 reg_low, uint16 *value)
{
    uint8 bytes[2];

    if (value == 0)
    {
        return 0;
    }
    // 从偶数低字节地址开始连续读取，低字节读取时芯片会锁存对应高字节
    if (!SoftI2C_ReadRegisters(bus, TCS3400_I2C_ADDRESS,
                               reg_low, bytes, 2u))
    {
        return 0;
    }

    *value = (uint16)bytes[0] | ((uint16)bytes[1] << 8); // 低地址字节在前
    return 1;
}

/**
 * @brief  写入并回读校验寄存器
 * @param  bus 总线编号
 * @param  reg 寄存器地址
 * @param  value 写入值
 * @param  mask 校验掩码
 * @retval 1：写入并校验成功；0：通信或校验错误
 */
static bit TCS3400_WriteVerified(SoftI2C_Bus_t bus, uint8 reg, uint8 value, uint8 mask)
{
    uint8 readback;

    if (!SoftI2C_WriteRegister(bus, TCS3400_I2C_ADDRESS, reg, value))
    {
        return 0;
    }
    if (!SoftI2C_ReadRegister(bus, TCS3400_I2C_ADDRESS, reg, &readback))
    {
        return 0;
    }

    return ((readback & mask) == (value & mask));
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化指定总线上的TCS3400
 * @param  bus 总线编号
 * @param  atime 默认ATIME寄存器值
 * @param  gain 默认增益寄存器值
 * @retval 1：初始化成功；0：ID、通信或校验错误
 */
bit TCS3400_Init(SoftI2C_Bus_t bus, uint8 atime, uint8 gain)
{
    uint8 id;

    if (!TCS3400_ReadID(bus, &id) ||
        ((id & TCS3400_DEVICE_ID_MASK) != TCS3400_EXPECTED_ID))
    {
        return 0;
    }
    if (!SoftI2C_WriteRegister(bus, TCS3400_I2C_ADDRESS,
                               TCS3400_REG_ENABLE, 0x00u))
    {
        return 0;
    }
    if (!TCS3400_WriteVerified(bus, TCS3400_REG_CONFIG,
                               TCS3400_CONFIG_REQUIRED, 0x40u))
    {
        return 0;
    }
    if (!TCS3400_SetIRMap(bus, 0))
    {
        return 0;
    }
    // 固件上电默认ATIME和增益必须真实写入，失败则整路初始化失败
    if (!TCS3400_SetATIME(bus, atime) || !TCS3400_SetGain(bus, gain))
    {
        return 0;
    }
    if (!SoftI2C_WriteRegister(bus, TCS3400_I2C_ADDRESS,
                               TCS3400_REG_ENABLE, TCS3400_ENABLE_PON))
    {
        return 0;
    }

    Delay_ms(3u);
    return SoftI2C_WriteRegister(bus, TCS3400_I2C_ADDRESS, TCS3400_REG_ENABLE,
                                 TCS3400_ENABLE_PON | TCS3400_ENABLE_AEN);
}

/**
 * @brief  设置并回读校验ATIME寄存器
 * @param  bus 总线编号
 * @param  atime ATIME寄存器值
 * @retval 1：设置成功；0：通信或校验错误
 */
bit TCS3400_SetATIME(SoftI2C_Bus_t bus, uint8 atime)
{
    return TCS3400_WriteVerified(bus, TCS3400_REG_ATIME, atime, 0xFFu);
}

/**
 * @brief  设置并回读校验增益寄存器
 * @param  bus 总线编号
 * @param  gain 增益寄存器值0至3
 * @retval 1：设置成功；0：参数、通信或校验错误
 */
bit TCS3400_SetGain(SoftI2C_Bus_t bus, uint8 gain)
{
    if (gain > 3u)
    {
        return 0;
    }

    return TCS3400_WriteVerified(bus, TCS3400_REG_CONTROL, gain, 0x03u);
}

/**
 * @brief  读取TCS3400芯片ID
 * @param  bus 总线编号
 * @param  id ID存放地址
 * @retval 1：读取成功；0：参数或通信错误
 */
bit TCS3400_ReadID(SoftI2C_Bus_t bus, uint8 *id)
{
    if (id == 0)
    {
        return 0;
    }

    return SoftI2C_ReadRegister(bus, TCS3400_I2C_ADDRESS, TCS3400_REG_ID, id);
}

/**
 * @brief  重新启动一帧积分
 * @param  bus 总线编号
 * @retval 1：启动成功；0：通信错误
 */
bit TCS3400_RestartIntegration(SoftI2C_Bus_t bus)
{
    // 先清AEN再重新置位，强制丢弃旧帧并开始一次新积分
    if (!SoftI2C_WriteRegister(bus, TCS3400_I2C_ADDRESS,
                               TCS3400_REG_ENABLE, TCS3400_ENABLE_PON))
    {
        return 0;
    }

    return SoftI2C_WriteRegister(bus, TCS3400_I2C_ADDRESS, TCS3400_REG_ENABLE,
                                 TCS3400_ENABLE_PON | TCS3400_ENABLE_AEN);
}

/**
 * @brief  读取数据有效状态
 * @param  bus 总线编号
 * @param  valid 数据有效标志存放地址
 * @retval 1：读取成功；0：参数或通信错误
 */
bit TCS3400_GetDataValid(SoftI2C_Bus_t bus, uint8 *valid)
{
    uint8 status;

    if ((valid == 0) ||
        !SoftI2C_ReadRegister(bus, TCS3400_I2C_ADDRESS, TCS3400_REG_STATUS, &status))
    {
        return 0;
    }

    *valid = (status & TCS3400_STATUS_AVALID) ? 1u : 0u; // AVALID置位表示新帧完成
    return 1;
}

/**
 * @brief  读取指定通道原始值
 * @param  bus 总线编号
 * @param  channel Clear、R、G或B通道
 * @param  value 原始值存放地址
 * @retval 1：读取成功；0：参数或通信错误
 */
bit TCS3400_ReadChannel(SoftI2C_Bus_t bus, Tcs3400Channel_t channel,
                        uint16 *value)
{
    uint8 reg_low;

    switch (channel)
    {
        case TCS3400_CHANNEL_CLEAR:
            reg_low = TCS3400_REG_CLEAR_LOW;
            break;
        case TCS3400_CHANNEL_RED:
            reg_low = TCS3400_REG_RED_LOW;
            break;
        case TCS3400_CHANNEL_GREEN:
            reg_low = TCS3400_REG_GREEN_LOW;
            break;
        case TCS3400_CHANNEL_BLUE:
            reg_low = TCS3400_REG_BLUE_LOW;
            break;
        default:
            return 0;
    }

    return TCS3400_ReadUint16(bus, reg_low, value);
}

/**
 * @brief  设置IR映射状态
 * @param  bus 总线编号
 * @param  enabled 1：开启；0：关闭
 * @retval 1：设置成功；0：通信或校验错误
 */
bit TCS3400_SetIRMap(SoftI2C_Bus_t bus, bit enabled)
{
    uint8 value;

    value = enabled ? TCS3400_IR_MAP_BIT : 0x00u; // 开启后Clear端口输出IR数据
    return TCS3400_WriteVerified(bus, TCS3400_REG_IR_MAP, value, 0xFFu);
}
