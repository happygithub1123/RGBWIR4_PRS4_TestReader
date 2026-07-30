#include "tcs3400_driver.h"
#include "Delay.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define TCS3400_I2C_ADDRESS       0x39u // 本项目四路TCS3400共用的7位I2C地址
#define TCS3400_EXPECTED_ID       0x90u // 本项目识别TCS3400型号使用的ID有效位
#define TCS3400_DEVICE_ID_MASK    0xFCu // 本项目校验ID时屏蔽非型号位

#define TCS3400_REG_ENABLE        0x80u // 本项目控制TCS3400上电和积分使能的寄存器
#define TCS3400_REG_ATIME         0x81u // 本项目写入六档积分时间的寄存器
#define TCS3400_REG_CONFIG        0x8Du // 本项目初始化TCS3400固定配置位的寄存器
#define TCS3400_REG_CONTROL       0x8Fu // 本项目写入四档增益的寄存器
#define TCS3400_REG_ID            0x92u // 本项目读取并校验芯片ID的寄存器
#define TCS3400_REG_STATUS        0x93u // 本项目读取AVALID状态的寄存器
#define TCS3400_REG_CLEAR_LOW     0x94u // 本项目读取Clear低字节及IR映射值的起始寄存器
#define TCS3400_REG_RED_LOW       0x96u // 本项目读取R低字节的起始寄存器
#define TCS3400_REG_GREEN_LOW     0x98u // 本项目读取G低字节的起始寄存器
#define TCS3400_REG_BLUE_LOW      0x9Au // 本项目读取B低字节的起始寄存器
#define TCS3400_REG_IR_MAP        0xC0u // 本项目切换IR到Clear映射的寄存器

#define TCS3400_ENABLE_PON        0x01u // 本项目保持TCS3400上电的ENABLE位
#define TCS3400_ENABLE_AEN        0x02u // 本项目启动环境光积分的ENABLE位
#define TCS3400_STATUS_AVALID     0x01u // 本项目判断新采样数据有效的状态位
#define TCS3400_IR_MAP_BIT        0x80u // 本项目把IR结果映射到Clear寄存器的控制位
#define TCS3400_CONFIG_REQUIRED   0x40u // 本项目初始化时必须写入并回读的固定配置位

/**
 * @brief  读取16位小端寄存器值
 * @param  bus 总线编号
 * @param  reg_low 低字节寄存器地址
 * @param  value 读取值存放地址
 * @retval 1：读取成功；0：参数或通信错误
 */
static bit TCS3400_ReadUint16(SoftI2C_Bus_t bus, uint8 reg_low, uint16 *value)
{
    // 本项目中该函数从偶数低字节地址连续读取一对寄存器并组合16位通道计数
    uint8 bytes[2];

    if (value == 0)
    {
        return 0;
    }
    // 低字节读取会锁存对应高字节，一次连续读取减少重复寻址并保持同一采样值
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
    // 本项目中该函数写入关键配置后立即回读，防止软件状态与真实寄存器不一致
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
    // 本项目中该函数按稳定main顺序校验ID并写入固定配置、积分时间和增益
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
    // 本项目中该函数写入并回读确认指定路TCS3400积分时间
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
    // 本项目中该函数限制四档增益范围并回读确认CONTROL低两位
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
    // 本项目中该函数从目标软件I2C总线读取真实TCS3400 ID
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
    // 本项目中该函数先关闭AEN再重新开启，确保后续读取使用一帧新数据
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
    // 本项目中该函数读取AVALID，确认当前积分帧可以交给控制层读取
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
 * @brief  读取指定普通环境光通道原始值
 * @param  bus 总线编号
 * @param  channel Clear、R、G或B通道
 * @param  value 原始值存放地址
 * @retval 1：读取成功；0：参数或通信错误
 */
bit TCS3400_ReadChannel(SoftI2C_Bus_t bus, Tcs3400Channel_t channel,
                        uint16 *value)
{
    // 本项目中该函数只选择当前Pin1需要的通道，减少无关I2C寄存器访问
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

    // 仍严格从偶数低字节开始，并按低字节、高字节顺序组合
    return TCS3400_ReadUint16(bus, reg_low, value);
}

/**
 * @brief  读取Clear数据口原始值
 * @param  bus 总线编号
 * @param  value 原始值存放地址
 * @retval 1：读取成功；0：参数或通信错误
 */
bit TCS3400_ReadClearPort(SoftI2C_Bus_t bus, uint16 *value)
{
    // 本项目中该函数读取Clear端口，IR映射开启时该端口返回IR计数
    return TCS3400_ReadUint16(bus, TCS3400_REG_CLEAR_LOW, value);
}

/**
 * @brief  设置IR映射状态
 * @param  bus 总线编号
 * @param  enabled 1：开启；0：关闭
 * @retval 1：设置成功；0：通信或校验错误
 */
bit TCS3400_SetIRMap(SoftI2C_Bus_t bus, bit enabled)
{
    // 本项目中该函数开启或关闭IR映射，并通过回读确认映射真实生效
    uint8 value;

    value = enabled ? TCS3400_IR_MAP_BIT : 0x00u; // 开启后Clear端口输出IR数据
    return TCS3400_WriteVerified(bus, TCS3400_REG_IR_MAP, value, 0xFFu);
}
