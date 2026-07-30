#include "mtf_protocol.h"
#include "Uart1.h"

/*==============================================================================
 内部定义
 *============================================================================*/

typedef union
{
    float value;    // 本项目中保存准备返回ICT的环境光或压力Float值
    uint8 bytes[4]; // 本项目中按字节访问Keil C51的Float内存排列
} MtfFloatBytes_t; // 本项目中用于把C51 Float重排成MTF协议小端载荷

/**
 * @brief  判断字符是否为十进制数字
 * @param  dat 待判断字符
 * @retval 1：是数字；0：不是数字
 */
static bit MtfProtocol_IsDigit(uint8 dat)
{
    // 本项目中该函数只允许Pin1、Pin2和工作值2出现ASCII十进制数字
    return ((dat >= '0') && (dat <= '9'));
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  解析固定15字节MTF请求帧
 * @param  buffer 请求帧缓冲区
 * @param  length 请求帧长度
 * @param  request 解析结果存放地址
 * @retval 1：解析成功；0：格式错误
 */
bit MtfProtocol_ParseFrame(const uint8 *buffer, uint8 length, MtfRequest_t *request)
{
    // 本项目中该函数严格校验15字节MTF格式，并且只解析Pin1和工作值2
    // 本项目只接受15字节MTF请求，指针或长度错误时禁止访问协议下标
    if ((buffer == 0) || (request == 0) ||
        (length != MTF_REQUEST_FRAME_LENGTH))
    {
        return 0;
    }

    // 校验ICT命令固定帧头、两个分隔符和真实回车换行位置
    if ((buffer[0] != 'M') || (buffer[1] != 'T') ||
        (buffer[2] != 'F') || (buffer[3] != ':') ||
        (buffer[6] != '-') || (buffer[9] != '-') ||
        (buffer[13] != '\r') || (buffer[14] != '\n'))
    {
        return 0;
    }

    // Pin1、Pin2和工作值2必须全为数字，Pin2只校验格式而不进入业务层
    if (!MtfProtocol_IsDigit(buffer[MTF_FRAME_TARGET_CODE_INDEX]) ||
        !MtfProtocol_IsDigit(buffer[MTF_FRAME_FUNCTION_CODE_INDEX]) ||
        !MtfProtocol_IsDigit(buffer[MTF_FRAME_RESERVED_TENS_INDEX]) ||
        !MtfProtocol_IsDigit(buffer[MTF_FRAME_RESERVED_ONES_INDEX]) ||
        !MtfProtocol_IsDigit(buffer[MTF_FRAME_PARAMETER_HUNDREDS_INDEX]) ||
        !MtfProtocol_IsDigit(buffer[MTF_FRAME_PARAMETER_TENS_INDEX]) ||
        !MtfProtocol_IsDigit(buffer[MTF_FRAME_PARAMETER_ONES_INDEX]))
    {
        return 0;
    }

    request->command_code =
        (uint8)(((buffer[MTF_FRAME_TARGET_CODE_INDEX] - '0') * 10u) +
                 (buffer[MTF_FRAME_FUNCTION_CODE_INDEX] - '0'));
    request->parameter =
        (uint16)(((buffer[MTF_FRAME_PARAMETER_HUNDREDS_INDEX] - '0') * 100u) +
                 ((buffer[MTF_FRAME_PARAMETER_TENS_INDEX] - '0') * 10u) +
                  (buffer[MTF_FRAME_PARAMETER_ONES_INDEX] - '0'));
    return 1;
}

/**
 * @brief  发送Float响应帧
 * @param  value 返回数值
 * @retval 1：发送成功；0：UART1发送超时
 */
bit MtfProtocol_SendValue(float value)
{
    // 组帧逻辑只有这一处调用，直接放在发送函数中避免多余接口
    MtfFloatBytes_t float_bytes;
    uint8 frame[MTF_RESPONSE_FRAME_LENGTH];

    float_bytes.value = value;
    frame[0] = '=';
    // Keil C51的Float内存顺序与ICT载荷顺序相反，需要显式重排
    frame[1] = float_bytes.bytes[3];
    frame[2] = float_bytes.bytes[2];
    frame[3] = float_bytes.bytes[1];
    frame[4] = float_bytes.bytes[0];
    frame[5] = '\r';
    frame[6] = '\n';
    return Uart1_SendBuffer(frame, MTF_RESPONSE_FRAME_LENGTH);
}

/**
 * @brief  发送统一异常响应帧
 * @param  无
 * @retval 1：发送成功；0：UART1发送超时
 */
bit MtfProtocol_SendError(void)
{
    // 本项目中该函数把所有上层错误统一转换为ICT可识别的Float负一响应
    return MtfProtocol_SendValue(-1.0f); // 本项目所有格式、参数和硬件异常统一返回Float负一
}
