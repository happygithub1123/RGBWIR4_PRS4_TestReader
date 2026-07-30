#ifndef __MTF_PROTOCOL_H__
#define __MTF_PROTOCOL_H__ // 防止本项目MTF协议接口被重复包含

#include "Public_cfg.h"

typedef struct
{
    uint8 command_code; // 本项目中保存从Pin1两个数字解析出的AB指令码
    uint16 parameter;   // 本项目中保存从工作值2三个数字解析出的EFG参数
} MtfRequest_t; // 本项目中保存一条已通过格式校验并准备交给传感器控制层的MTF请求

bit MtfProtocol_ParseFrame(const uint8 *buffer, uint8 length, MtfRequest_t *request); // 将本项目15字节MTF请求解析为AB和EFG
bit MtfProtocol_SendValue(float value); // 通过UART1发送本项目正常Float响应
bit MtfProtocol_SendError(void); // 通过UART1发送本项目统一的Float负一异常响应

#endif
