#include "soft_i2c.h"
#include <intrins.h>

/*==============================================================================
 内部定义
 *============================================================================*/

#define SOFT_I2C_START_SETUP_DELAY  180u // 四路TCS3400发送START前保证SDA和SCL稳定为高的延时次数
#define SOFT_I2C_START_HOLD_DELAY   144u // 四路TCS3400产生START后保持起始条件的延时次数
#define SOFT_I2C_STOP_SETUP_DELAY   144u // 四路TCS3400产生STOP前保持SDA为低的延时次数
#define SOFT_I2C_BUS_FREE_DELAY     180u // 两次TCS3400软件I2C事务之间释放总线的延时次数
#define SOFT_I2C_DATA_SETUP_DELAY    36u // TCS3400每个数据位在SCL上升前建立电平的延时次数
#define SOFT_I2C_CLOCK_HIGH_DELAY   144u // TCS3400软件I2C时钟高电平阶段的延时次数
#define SOFT_I2C_CLOCK_LOW_DELAY    180u // TCS3400软件I2C时钟低电平阶段的延时次数

sbit SOFT_I2C_1_SCL = P0 ^ 4;
sbit SOFT_I2C_1_SDA = P5 ^ 2;
sbit SOFT_I2C_2_SCL = P0 ^ 1;
sbit SOFT_I2C_2_SDA = P0 ^ 2;
sbit SOFT_I2C_3_SCL = P4 ^ 5;
sbit SOFT_I2C_3_SDA = P4 ^ 6;
sbit SOFT_I2C_4_SCL = P2 ^ 5;
sbit SOFT_I2C_4_SDA = P2 ^ 6;

/**
 * @brief  执行软件I2C时序延时
 * @param  count 延时循环次数
 * @retval 无
 */
static void SoftI2C_Delay(uint16 count)
{
    // 本项目中该函数提供四路TCS3400软件I2C各时序阶段需要的短延时
    while (count--)
    {
        _nop_();
    }
}

/**
 * @brief  判断软件I2C总线编号是否有效
 * @param  bus 总线编号
 * @retval 1：有效；0：无效
 */
static bit SoftI2C_IsValidBus(SoftI2C_Bus_t bus)
{
    // 本项目中该函数只允许访问实际连接四路TCS3400的有效软件I2C总线
    return ((uint8)bus < (uint8)SOFT_I2C_BUS_COUNT);
}

/**
 * @brief  设置指定总线SCL电平
 * @param  bus 总线编号
 * @param  level 目标电平
 * @retval 无
 */
static void SoftI2C_SetScl(SoftI2C_Bus_t bus, bit level)
{
    // 本项目中该函数按传感器路数控制对应GPIO上的软件I2C时钟线
    switch (bus)
    {
        case SOFT_I2C_BUS_1: SOFT_I2C_1_SCL = level; break;
        case SOFT_I2C_BUS_2: SOFT_I2C_2_SCL = level; break;
        case SOFT_I2C_BUS_3: SOFT_I2C_3_SCL = level; break;
        case SOFT_I2C_BUS_4: SOFT_I2C_4_SCL = level; break;
        default: break;
    }
}

/**
 * @brief  设置指定总线SDA电平
 * @param  bus 总线编号
 * @param  level 目标电平
 * @retval 无
 */
static void SoftI2C_SetSda(SoftI2C_Bus_t bus, bit level)
{
    // 本项目中该函数按传感器路数驱动或释放对应GPIO上的软件I2C数据线
    switch (bus)
    {
        case SOFT_I2C_BUS_1: SOFT_I2C_1_SDA = level; break;
        case SOFT_I2C_BUS_2: SOFT_I2C_2_SDA = level; break;
        case SOFT_I2C_BUS_3: SOFT_I2C_3_SDA = level; break;
        case SOFT_I2C_BUS_4: SOFT_I2C_4_SDA = level; break;
        default: break;
    }
}

/**
 * @brief  读取指定总线SDA电平
 * @param  bus 总线编号
 * @retval SDA电平
 */
static bit SoftI2C_GetSda(SoftI2C_Bus_t bus)
{
    // 本项目中该函数读取目标TCS3400数据线，用于接收ACK和寄存器数据位
    switch (bus)
    {
        case SOFT_I2C_BUS_1: return SOFT_I2C_1_SDA;
        case SOFT_I2C_BUS_2: return SOFT_I2C_2_SDA;
        case SOFT_I2C_BUS_3: return SOFT_I2C_3_SDA;
        case SOFT_I2C_BUS_4: return SOFT_I2C_4_SDA;
        default: return 1;
    }
}

/**
 * @brief  产生软件I2C开始信号
 * @param  bus 总线编号
 * @retval 无
 */
static void SoftI2C_Start(SoftI2C_Bus_t bus)
{
    // 本项目中该函数在目标TCS3400总线上产生符合I2C时序的START条件
    SoftI2C_SetSda(bus, 1); // 释放目标TCS3400总线SDA，准备产生START
    SoftI2C_SetScl(bus, 1); // 拉高目标TCS3400总线SCL，进入START建立阶段
    SoftI2C_Delay(SOFT_I2C_START_SETUP_DELAY);
    SoftI2C_SetSda(bus, 0); // 在SCL高电平期间拉低SDA，通知目标TCS3400开始事务
    SoftI2C_Delay(SOFT_I2C_START_HOLD_DELAY);
    SoftI2C_SetScl(bus, 0);
}

/**
 * @brief  产生软件I2C停止信号
 * @param  bus 总线编号
 * @retval 无
 */
static void SoftI2C_Stop(SoftI2C_Bus_t bus)
{
    // 本项目中该函数在目标TCS3400总线上产生STOP并释放本次寄存器事务
    SoftI2C_SetSda(bus, 0);
    SoftI2C_SetScl(bus, 1);
    SoftI2C_Delay(SOFT_I2C_STOP_SETUP_DELAY);
    SoftI2C_SetSda(bus, 1); // 在SCL高电平期间释放SDA，结束本次TCS3400事务
    SoftI2C_Delay(SOFT_I2C_BUS_FREE_DELAY);
}

/**
 * @brief  等待从机应答
 * @param  bus 总线编号
 * @retval 1：收到ACK；0：收到NACK
 */
static bit SoftI2C_WaitAck(SoftI2C_Bus_t bus)
{
    // 本项目中该函数读取第9个时钟的ACK，判断目标TCS3400是否响应当前字节
    bit acknowledged;

    SoftI2C_SetSda(bus, 1); // 释放SDA，让目标TCS3400在第9个时钟返回ACK
    SoftI2C_Delay(SOFT_I2C_DATA_SETUP_DELAY);
    SoftI2C_SetScl(bus, 1);
    SoftI2C_Delay(SOFT_I2C_CLOCK_HIGH_DELAY);
    acknowledged = (SoftI2C_GetSda(bus) == 0); // SDA为低表示目标TCS3400已接收当前字节
    SoftI2C_SetScl(bus, 0);
    SoftI2C_Delay(SOFT_I2C_CLOCK_LOW_DELAY);
    return acknowledged;
}

/**
 * @brief  发送一个软件I2C字节
 * @param  bus 总线编号
 * @param  dat 待发送字节
 * @retval 1：从机应答；0：从机未应答
 */
static bit SoftI2C_SendByte(SoftI2C_Bus_t bus, uint8 dat)
{
    // 本项目中该函数按最高位优先发送一个TCS3400地址、寄存器或数据字节
    uint8 bit_index;

    // 按TCS3400 I2C时序从最高位发送，并只在SCL低电平期间改变SDA
    for (bit_index = 0u; bit_index < 8u; bit_index++)
    {
        SoftI2C_SetSda(bus, (dat & 0x80u) != 0u);
        SoftI2C_Delay(SOFT_I2C_DATA_SETUP_DELAY);
        SoftI2C_SetScl(bus, 1);
        SoftI2C_Delay(SOFT_I2C_CLOCK_HIGH_DELAY);
        SoftI2C_SetScl(bus, 0);
        SoftI2C_Delay(SOFT_I2C_CLOCK_LOW_DELAY);
        dat <<= 1;
    }

    SoftI2C_SetSda(bus, 1);
    return SoftI2C_WaitAck(bus);
}

/**
 * @brief  读取一个软件I2C字节
 * @param  bus 总线编号
 * @param  acknowledge 1：发送ACK；0：发送NACK
 * @retval 接收到的字节
 */
static uint8 SoftI2C_ReadByte(SoftI2C_Bus_t bus, bit acknowledge)
{
    // 本项目中该函数读取一个TCS3400寄存器字节，并按是否继续读取返回ACK或NACK
    uint8 bit_index;
    uint8 dat;

    dat = 0u;
    SoftI2C_SetSda(bus, 1); // 释放SDA后由目标TCS3400依次输出寄存器数据位

    for (bit_index = 0u; bit_index < 8u; bit_index++)
    {
        dat <<= 1;
        SoftI2C_SetScl(bus, 1);
        SoftI2C_Delay(SOFT_I2C_CLOCK_HIGH_DELAY);
        if (SoftI2C_GetSda(bus))
        {
            dat |= 0x01u;
        }
        SoftI2C_SetScl(bus, 0);
        SoftI2C_Delay(SOFT_I2C_CLOCK_LOW_DELAY);
    }

    SoftI2C_SetSda(bus, acknowledge ? 0 : 1); // 连续读低字节后回ACK，读完高字节后回NACK结束
    SoftI2C_Delay(SOFT_I2C_DATA_SETUP_DELAY);
    SoftI2C_SetScl(bus, 1);
    SoftI2C_Delay(SOFT_I2C_CLOCK_HIGH_DELAY);
    SoftI2C_SetScl(bus, 0);
    SoftI2C_SetSda(bus, 1);
    SoftI2C_Delay(SOFT_I2C_CLOCK_LOW_DELAY);
    return dat;
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化并释放四路软件I2C总线
 * @param  无
 * @retval 无
 */
void SoftI2C_Init(void)
{
    // 本项目中该函数释放四路TCS3400总线并建立上电后的首次总线空闲时间
    // 主板上电时释放四路TCS3400的SCL和SDA，为首次START建立空闲时间
    SoftI2C_SetSda(SOFT_I2C_BUS_1, 1); SoftI2C_SetScl(SOFT_I2C_BUS_1, 1);
    SoftI2C_SetSda(SOFT_I2C_BUS_2, 1); SoftI2C_SetScl(SOFT_I2C_BUS_2, 1);
    SoftI2C_SetSda(SOFT_I2C_BUS_3, 1); SoftI2C_SetScl(SOFT_I2C_BUS_3, 1);
    SoftI2C_SetSda(SOFT_I2C_BUS_4, 1); SoftI2C_SetScl(SOFT_I2C_BUS_4, 1);
    SoftI2C_Delay(SOFT_I2C_BUS_FREE_DELAY);
}

/**
 * @brief  写入从机单个寄存器
 * @param  bus 总线编号
 * @param  address 从机7位地址
 * @param  reg 寄存器地址
 * @param  value 写入值
 * @retval 1：写入成功；0：参数或通信错误
 */
bit SoftI2C_WriteRegister(SoftI2C_Bus_t bus, uint8 address, uint8 reg, uint8 value)
{
    // 本项目中该函数完成一次目标TCS3400单寄存器写事务，任一NACK都立即结束
    // 传感器路数非法时不操作GPIO，避免误访问其他TCS3400总线
    if (!SoftI2C_IsValidBus(bus))
    {
        return 0;
    }

    SoftI2C_Start(bus);
    if (!SoftI2C_SendByte(bus, (uint8)(address << 1)))
    {
        SoftI2C_Stop(bus);
        return 0;
    }
    if (!SoftI2C_SendByte(bus, reg))
    {
        SoftI2C_Stop(bus);
        return 0;
    }
    if (!SoftI2C_SendByte(bus, value))
    {
        SoftI2C_Stop(bus);
        return 0;
    }

    SoftI2C_Stop(bus);
    return 1;
}

/**
 * @brief  读取从机单个寄存器
 * @param  bus 总线编号
 * @param  address 从机7位地址
 * @param  reg 寄存器地址
 * @param  value 读取值存放地址
 * @retval 1：读取成功；0：参数或通信错误
 */
bit SoftI2C_ReadRegister(SoftI2C_Bus_t bus, uint8 address, uint8 reg, uint8 *value)
{
    // 本项目中该函数复用连续读取接口取得一个TCS3400寄存器字节
    return SoftI2C_ReadRegisters(bus, address, reg, value, 1u);
}

/**
 * @brief  连续读取从机寄存器
 * @param  bus 总线编号
 * @param  address 从机7位地址
 * @param  reg 起始寄存器地址
 * @param  buffer 读取缓冲区
 * @param  length 读取长度
 * @retval 1：读取成功；0：参数或通信错误
 */
bit SoftI2C_ReadRegisters(SoftI2C_Bus_t bus, uint8 address, uint8 reg,
                          uint8 *buffer, uint8 length)
{
    // 本项目中该函数用重复START连续读取TCS3400寄存器，保证通道低高字节连续
    uint8 index;

    if (!SoftI2C_IsValidBus(bus) || (buffer == 0) || (length == 0u))
    {
        return 0;
    }

    // 先以写方向发送目标TCS3400寄存器地址，为后续连续读取定位起点
    SoftI2C_Start(bus);
    if (!SoftI2C_SendByte(bus, (uint8)(address << 1)))
    {
        SoftI2C_Stop(bus);
        return 0;
    }
    if (!SoftI2C_SendByte(bus, reg))
    {
        SoftI2C_Stop(bus);
        return 0;
    }

    // 用重复START切换到读方向，使TCS3400保持当前寄存器地址连续输出
    SoftI2C_Start(bus);
    if (!SoftI2C_SendByte(bus, (uint8)((address << 1) | 0x01u)))
    {
        SoftI2C_Stop(bus);
        return 0;
    }

    // 连续读取时中间字节回ACK，最后字节回NACK并发送STOP结束TCS3400事务
    for (index = 0u; index < length; index++)
    {
        buffer[index] = SoftI2C_ReadByte(bus, index < (uint8)(length - 1u));
    }

    SoftI2C_Stop(bus);
    return 1;
}
