#include "soft_i2c.h"
#include <intrins.h>

/*==============================================================================
 内部定义
 *============================================================================*/

#define SOFT_I2C_START_SETUP_DELAY  180u
#define SOFT_I2C_START_HOLD_DELAY   144u
#define SOFT_I2C_STOP_SETUP_DELAY   144u
#define SOFT_I2C_BUS_FREE_DELAY     180u
#define SOFT_I2C_DATA_SETUP_DELAY    36u
#define SOFT_I2C_CLOCK_HIGH_DELAY   144u
#define SOFT_I2C_CLOCK_LOW_DELAY    180u

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
    SoftI2C_SetSda(bus, 1); // 先释放数据线
    SoftI2C_SetScl(bus, 1); // 确保时钟线为高
    SoftI2C_Delay(SOFT_I2C_START_SETUP_DELAY);
    SoftI2C_SetSda(bus, 0); // SCL高电平期间SDA下降形成START
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
    SoftI2C_SetSda(bus, 0);
    SoftI2C_SetScl(bus, 1);
    SoftI2C_Delay(SOFT_I2C_STOP_SETUP_DELAY);
    SoftI2C_SetSda(bus, 1); // SCL高电平期间SDA上升形成STOP
    SoftI2C_Delay(SOFT_I2C_BUS_FREE_DELAY);
}

/**
 * @brief  等待从机应答
 * @param  bus 总线编号
 * @retval 1：收到ACK；0：收到NACK
 */
static bit SoftI2C_WaitAck(SoftI2C_Bus_t bus)
{
    bit acknowledged;

    SoftI2C_SetSda(bus, 1); // 释放SDA，由从机在第9个时钟拉低
    SoftI2C_Delay(SOFT_I2C_DATA_SETUP_DELAY);
    SoftI2C_SetScl(bus, 1);
    SoftI2C_Delay(SOFT_I2C_CLOCK_HIGH_DELAY);
    acknowledged = (SoftI2C_GetSda(bus) == 0); // 低电平表示ACK
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
    uint8 bit_index;

    // 按MSB优先发送，每位在SCL低电平期间更新SDA
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
    uint8 bit_index;
    uint8 dat;

    dat = 0u;
    SoftI2C_SetSda(bus, 1); // 释放SDA后由从机驱动数据位

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

    SoftI2C_SetSda(bus, acknowledge ? 0 : 1); // 非末字节回ACK，末字节回NACK
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
    // 四路均释放为高电平，建立首次START前的总线空闲时间
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
    // 总线编号非法时禁止操作任何GPIO
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
    uint8 index;

    if (!SoftI2C_IsValidBus(bus) || (buffer == 0) || (length == 0u))
    {
        return 0;
    }

    // 先用写方向设置目标寄存器地址
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

    // 重复START切换到读方向，保持同一总线事务
    SoftI2C_Start(bus);
    if (!SoftI2C_SendByte(bus, (uint8)((address << 1) | 0x01u)))
    {
        SoftI2C_Stop(bus);
        return 0;
    }

    // 除最后一字节外均回ACK，最后一字节回NACK结束读取
    for (index = 0u; index < length; index++)
    {
        buffer[index] = SoftI2C_ReadByte(bus, index < (uint8)(length - 1u));
    }

    SoftI2C_Stop(bus);
    return 1;
}
