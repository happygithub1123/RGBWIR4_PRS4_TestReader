#include "action_control.h"

/* X输入检测引脚定义 */
sbit X1_PIN = P5^1;
sbit X2_PIN = P3^5;
sbit X3_PIN = P3^6;
sbit X4_PIN = P3^7;

/* Y输出引脚定义 */
sbit Y1_PIN = P3^2;
sbit Y2_PIN = P3^3;
sbit Y3_PIN = P3^4;
sbit Y4_PIN = P5^0;



/**
 * @brief 初始化Y输出
 */
void Yn_Init(void)
{
    Y_AllOff();
}

/**
 * @brief 控制指定Y输出
 * @param yn Y1~Y4
 * @param state Y_ON或Y_OFF
 */
void Yn_Control(unsigned char yn, unsigned char state)
{
    switch(yn)
    {
        case Y1:
            Y1_PIN = state;
            break;

        case Y2:
            Y2_PIN = state;
            break;

        case Y3:
            Y3_PIN = state;
            break;

        case Y4:
            Y4_PIN = state;
            break;

        default:
            break;
    }
}

/**
 * @brief 全开Y输出
void Y_AllOn(void)
{
    Y1_PIN = Y_ON;
    Y2_PIN = Y_ON;
    Y3_PIN = Y_ON;
    Y4_PIN = Y_ON;
}

/**
 * @brief 全关Y输出
 */
void Y_AllOff(void)
{
    Y1_PIN = Y_OFF;
    Y2_PIN = Y_OFF;
    Y3_PIN = Y_OFF;
    Y4_PIN = Y_OFF;
}

/**
 * @brief 读取指定X输入
 * @param xn X1~X4
 * @return 0检测到或1未检测到
 */
unsigned char Xn_Read(unsigned char xn)
{
    switch(xn)
    {
        case X1:
            return X1_PIN;

        case X2:
            return X2_PIN;

        case X3:
            return X3_PIN;

        case X4:
            return X4_PIN;

        default:
            return X_NOT_DETECTED;
    }
}

/**
 * @brief 根据X输入控制对应Y输出
 */
void XY_Control(void)
{
    if(Xn_Read(X1) == X_DETECTED)
    {
        Yn_Control(Y1, Y_ON);
    }
    else
    {
        Yn_Control(Y1, Y_OFF);
    }

    if(Xn_Read(X2) == X_DETECTED)
    {
        Yn_Control(Y2, Y_ON);
    }
    else
    {
        Yn_Control(Y2, Y_OFF);
    }

    if(Xn_Read(X3) == X_DETECTED)
    {
        Yn_Control(Y3, Y_ON);
    }
    else
    {
        Yn_Control(Y3, Y_OFF);
    }

    if(Xn_Read(X4) == X_DETECTED)
    {
        Yn_Control(Y4, Y_ON);
    }
    else
    {
        Yn_Control(Y4, Y_OFF);
    }
}

