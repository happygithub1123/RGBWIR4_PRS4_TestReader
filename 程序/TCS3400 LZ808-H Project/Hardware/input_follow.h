#ifndef __INPUT_FOLLOW_H__
#define __INPUT_FOLLOW_H__

#include "Public_cfg.h"

void InputFollow_Init(void); /* 初始化四路跟随输出。 */
void InputFollow_Run(void);  /* 更新四路输入到输出跟随状态。 */

#endif
