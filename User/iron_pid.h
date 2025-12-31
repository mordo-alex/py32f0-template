#ifndef __IRON_PID_H
#define __IRON_PID_H

#include <stdint.h>

// PID 参数结构体
typedef struct {
    double Kp;           // 比例系数
    double Ki;           // 积分系数
    double Kd;           // 微分系数
    double limMin;       // 输出下限 (0)
    double limMax;       // 输出上限 (PWM满占空比, 如 1000)
    double limMinInt;    // 积分限制下限
    double limMaxInt;    // 积分限制上限
    double T;            // 采样周期 (秒)
    
    // 运行时变量
    double integrator;   // 积分累加值
    double prevError;    // 上一次误差
    double differentiator;
    double prevMeasurement;
    double out;          // 计算结果
} PIDController;

void PID_Init(PIDController *pid);
double PID_Compute(PIDController *pid, double setpoint, double measurement);

#endif
