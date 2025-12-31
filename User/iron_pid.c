#include "iron_pid.h"

void PID_Init(PIDController *pid) {
    pid->integrator = 0.0f;
    pid->prevError  = 0.0f;
    pid->differentiator  = 0.0f;
    pid->prevMeasurement = 0.0f;
    pid->out = 0.0f;
}

double PID_Compute(PIDController *pid, double setpoint, double measurement) {
    double error = setpoint - measurement;

    // 1. 比例项 (Proportional)
    double proportional = pid->Kp * error;

    // 2. 积分项 (Integral)
    pid->integrator = pid->integrator + 0.5f * pid->Ki * pid->T * (error + pid->prevError);

    // 积分限幅 (防止积分饱和/Windup)
    if (pid->integrator > pid->limMaxInt) {
        pid->integrator = pid->limMaxInt;
    } else if (pid->integrator < pid->limMinInt) {
        pid->integrator = pid->limMinInt;
    }

    // 3. 微分项 (Derivative) - 这里的实现使用了测量值的变化，而不是误差的变化，以防止设定值突变时的冲击
    pid->differentiator = -(2.0f * pid->Kd * (measurement - pid->prevMeasurement) 
                        + (2.0f * 0.01f - pid->T) * pid->differentiator) 
                        / (2.0f * 0.01f + pid->T); // 0.01f 是低通滤波系数，可调

    // 总输出
    pid->out = proportional + pid->integrator + pid->differentiator;

    // 输出限幅
    if (pid->out > pid->limMax) {
        pid->out = pid->limMax;
    } else if (pid->out < pid->limMin) {
        pid->out = pid->limMin;
    }

    pid->prevError       = error;
    pid->prevMeasurement = measurement;

    return pid->out;
}
