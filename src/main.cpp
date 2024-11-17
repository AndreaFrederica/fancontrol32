/*
 * @Author: AndreaFrederica
 * @Description: Fan speed calculation test with curve fitting and PWM output
 */

#include <Arduino.h>
#include <ArduinoEigenDense.h> // Eigen library for matrix operations
// #include "STM32_PWM.h"

HardwareSerial serial1(PA10, PA9); // 使用 UART1 (PA10 作为 RX, PA9 作为 TX)
HardwareTimer *FanTimer = new HardwareTimer(TIM3); // 使用 TIM3 作为 PWM 定时器

// 引脚定义
int adcPin = A0;   // A0 (PA0) 作为 ADC 输入
int pwmPin = PB0;  // PB0 作为 PWM 输出
int adcValue = 0;
float voltage = 0.0;

// 曲线拟合数据
const int dataCount = 10;
float voltageData[dataCount] = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 13.5, 13.8, 14.0};
float fanPercentData[dataCount] = {0, 10, 20, 30, 50, 70, 80, 90, 95, 100};
float pwmPercentData[dataCount] = {50, 55, 60, 65, 70, 75, 80, 85, 90, 95};

// 曲线拟合系数
float voltageToFanPercentCoeffs[3];
float fanPercentToPWMCoeffs[3];

// 曲线拟合函数
void fitCurve(float xData[], float yData[], int count, float coeffs[]) {
    Eigen::MatrixXf A(count, 3);
    Eigen::VectorXf b(count);

    for (int i = 0; i < count; i++) {
        A(i, 0) = xData[i] * xData[i]; // x^2
        A(i, 1) = xData[i];            // x
        A(i, 2) = 1;                   // 常数项
        b(i) = yData[i];               // y
    }

    // 求解系数
    Eigen::VectorXf result = A.colPivHouseholderQr().solve(b);
    coeffs[0] = result(0);
    coeffs[1] = result(1);
    coeffs[2] = result(2);
}

// 计算给定 x 值的 y 值
float calculateY(float x, float coeffs[]) {
    return coeffs[0] * x * x + coeffs[1] * x + coeffs[2];
}

// PWM 相关配置初始化
void setupPWM() {
    pinMode(pwmPin, OUTPUT);   // 设置 PWM 引脚为输出
    analogWriteFrequency(25000); // 初始化 PWM 频率为 25kHz
    analogWrite(pwmPin, 128);    // 初始占空比为 50% (128 / 255)
}


// PWM 更新函数，根据计算的 pwmDuty 动态调整占空比
void updatePWM(float pwmDuty) {
    // 将 pwmDuty 范围限制在 0 到 100 之间
    pwmDuty = constrain(pwmDuty, 0.0, 100.0);

    // 将 0-100% 的占空比转换为 0-255 的 PWM 输出值
    int pwmValue = (int)(pwmDuty * 255.0 / 100.0);

    // 输出 PWM 占空比
    analogWrite(pwmPin, pwmValue);

    // 输出调试信息
    serial1.print("Updated PWM Duty Cycle: ");
    serial1.print(pwmDuty, 1);        // 打印占空比百分比
    serial1.print(" %, PWM Value: ");
    serial1.println(pwmValue);        // 打印实际写入的 PWM 值
}

// 初始化函数
void setup() {
    serial1.begin(115200);
    while (!serial1);

    // 曲线拟合
    fitCurve(voltageData, fanPercentData, dataCount, voltageToFanPercentCoeffs);
    fitCurve(fanPercentData, pwmPercentData, dataCount, fanPercentToPWMCoeffs);

    // 打印拟合系数
    serial1.println("Voltage to Fan Percent Curve Coefficients:");
    serial1.print("a: "); serial1.print(voltageToFanPercentCoeffs[0]);
    serial1.print(", b: "); serial1.print(voltageToFanPercentCoeffs[1]);
    serial1.print(", c: "); serial1.println(voltageToFanPercentCoeffs[2]);

    serial1.println("Fan Percent to PWM Curve Coefficients:");
    serial1.print("a: "); serial1.print(fanPercentToPWMCoeffs[0]);
    serial1.print(", b: "); serial1.print(fanPercentToPWMCoeffs[1]);
    serial1.print(", c: "); serial1.println(fanPercentToPWMCoeffs[2]);

    // 初始化 PWM
    setupPWM();
    serial1.println("PWM initialized on PB0 with 25kHz frequency");
}

void loop() {
    // 读取 ADC 电压
    adcValue = analogRead(adcPin);
    voltage = adcValue * (3.3 / 1023.0) * 4.2; // 假设电压分压比为 1/4.2

    // 计算风扇速度百分比和 PWM 占空比
    float fanPercent = calculateY(voltage, voltageToFanPercentCoeffs);
    float pwmDuty = calculateY(fanPercent, fanPercentToPWMCoeffs);

    // 限制百分比在 0-100% 范围内
    fanPercent = constrain(fanPercent, 0, 100);
    pwmDuty = constrain(pwmDuty, 0, 100);

    // 更新 PWM 输出
    updatePWM(pwmDuty);

    // 打印电压、风扇速度百分比和 PWM 占空比
    serial1.print("Input Voltage: ");
    serial1.print(voltage, 2);
    serial1.print(" V, Fan Speed Percentage: ");
    serial1.print(fanPercent, 1);
    serial1.print(" %, PWM Duty Cycle: ");
    serial1.print(pwmDuty, 1);
    serial1.println(" %");

    delay(1000); // 每秒输出一次
}
