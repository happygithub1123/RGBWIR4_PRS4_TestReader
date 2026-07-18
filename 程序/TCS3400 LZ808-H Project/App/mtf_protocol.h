#ifndef __MTF_PROTOCOL_H__
#define __MTF_PROTOCOL_H__

#include "Public_cfg.h"

typedef struct
{
    uint8 command;
    uint16 parameter;
} MtfRequest_t;

bit MtfProtocol_ParseFrame(const uint8 *buffer, uint8 length, MtfRequest_t *request); /* 解析15字节请求帧。 */
void MtfProtocol_BuildFloatFrame(float value, uint8 *frame); /* 构造7字节Float响应。 */
bit MtfProtocol_SendValue(float value); /* 发送Float响应。 */
bit MtfProtocol_SendError(void); /* 发送Float为-1.0的统一异常响应。 */

#endif
