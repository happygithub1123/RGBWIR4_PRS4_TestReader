#ifndef __PROTOCOL_CONFIG_H__
#define __PROTOCOL_CONFIG_H__ // 防止本项目MTF协议常量被重复定义

#define MTF_REQUEST_FRAME_LENGTH          15u // 本项目MTF固定请求帧MTF:AB-CD-EFG加回车换行的总字节数
#define MTF_RESPONSE_FRAME_LENGTH         7u  // 本项目Float响应帧头、4字节数据和回车换行的总字节数
#define MTF_RECEIVE_TIMEOUT_MS            50u // UART1从收到MTF帧头起等待完整请求帧的最长时间

#define MTF_TARGET_CODE_TCS3400_MIN       1u // AB十位为1时选择第1路TCS3400
#define MTF_TARGET_CODE_TCS3400_MAX       4u // AB十位为4时选择第4路TCS3400
#define MTF_TARGET_CODE_PRESSURE          5u // AB十位为5时执行压力普通读取或清零
#define MTF_TARGET_CODE_PRESSURE_LIMIT_MIN 6u // AB十位为6时配置或比较第1组压力限值
#define MTF_TARGET_CODE_PRESSURE_LIMIT_MAX 9u // AB十位为9时配置或比较第4组压力限值

#define MTF_FUNCTION_CODE_PRESSURE_READ   0u // AB个位为0时读取EFG地址对应的压力值
#define MTF_FUNCTION_CODE_PRESSURE_ZERO   1u // AB个位为1时清零EFG地址对应的压力仪表
#define MTF_FUNCTION_CODE_LIMIT_LOWER     0u // 压力限值命令个位为0时设置该组下限
#define MTF_FUNCTION_CODE_LIMIT_UPPER     1u // 压力限值命令个位为1时设置该组上限
#define MTF_FUNCTION_CODE_LIMIT_READ      2u // 压力限值命令个位为2时读取压力并更新对应Y输出

#define PRESSURE_LIMIT_CHANNEL_COUNT      4u   // 本项目保存并控制Y1至Y4的压力限值组数
#define PRESSURE_LIMIT_VALUE_MAX          999u // 工作值2允许设置的最大整数压力限值

#define MTF_FRAME_TARGET_CODE_INDEX       4u  // 固定MTF请求中AB十位目标码所在的数组下标
#define MTF_FRAME_FUNCTION_CODE_INDEX     5u  // 固定MTF请求中AB个位功能码所在的数组下标
#define MTF_FRAME_RESERVED_TENS_INDEX     7u  // 固定MTF请求中CD十位保留码所在的数组下标
#define MTF_FRAME_RESERVED_ONES_INDEX     8u  // 固定MTF请求中CD个位保留码所在的数组下标
#define MTF_FRAME_PARAMETER_HUNDREDS_INDEX 10u // 固定MTF请求中EFG百位参数所在的数组下标
#define MTF_FRAME_PARAMETER_TENS_INDEX    11u // 固定MTF请求中EFG十位参数所在的数组下标
#define MTF_FRAME_PARAMETER_ONES_INDEX    12u // 固定MTF请求中EFG个位参数所在的数组下标

#endif
