#include "mtf_protocol.h"
#include "Uart1.h"

/*==============================================================================
 内部定义
 *============================================================================*/

typedef union
{
    float value;
    uint8 bytes[4];
} MtfFloatBytes_t;

/**
 * @brief  判断字符是否为十进制数字
 * @param  dat 待判断字符
 * @retval 1：是数字；0：不是数字
 */
static bit MtfProtocol_IsDigit(uint8 dat)
{
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
    // 固定15字节协议，空指针或长度错误时禁止继续索引缓冲区
    if ((buffer == 0) || (request == 0) ||
        (length != MTF_REQUEST_FRAME_LENGTH))
    {
        return 0;
    }

    // 校验固定帧头、分隔符和真实回车换行位置
    if ((buffer[0] != 'M') || (buffer[1] != 'T') ||
        (buffer[2] != 'F') || (buffer[3] != ':') ||
        (buffer[6] != '-') || (buffer[9] != '-') ||
        (buffer[13] != '\r') || (buffer[14] != '\n'))
    {
        return 0;
    }

    // AB、CD、EFG必须全为数字，CD只校验格式而不保存
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
 * @brief  构造固定7字节小端Float响应帧
 * @param  value 待打包数值
 * @param  frame 响应帧存放地址
 * @retval 无
 */
void MtfProtocol_BuildFloatFrame(float value, uint8 *frame)
{
    MtfFloatBytes_t float_bytes;

    if (frame == 0)
    {
        return;
    }

    float_bytes.value = value;
    frame[0] = '=';                    // 固定响应帧头
    // Keil C51内存字节逆序写入，形成协议要求的小端Float载荷
    frame[1] = float_bytes.bytes[3];
    frame[2] = float_bytes.bytes[2];
    frame[3] = float_bytes.bytes[1];
    frame[4] = float_bytes.bytes[0];
    frame[5] = '\r';
    frame[6] = '\n';
}

/**
 * @brief  发送Float响应帧
 * @param  value 返回数值
 * @retval 1：发送成功；0：UART1发送超时
 */
bit MtfProtocol_SendValue(float value)
{
    uint8 frame[MTF_RESPONSE_FRAME_LENGTH];

    MtfProtocol_BuildFloatFrame(value, frame);
    return Uart1_SendBuffer(frame, MTF_RESPONSE_FRAME_LENGTH);
}

/**
 * @brief  发送统一异常响应帧
 * @param  无
 * @retval 1：发送成功；0：UART1发送超时
 */
bit MtfProtocol_SendError(void)
{
    return MtfProtocol_SendValue(-1.0f); // 主板统一按Float负一识别异常
}
