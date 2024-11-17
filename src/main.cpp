/*
 * @Author: AndreaFrederica andreafrederica@outlook.com
 * @Date: 2024-09-19 18:46:03
 * @LastEditors: AndreaFrederica andreafrederica@outlook.com
 * @LastEditTime: 2024-09-23 20:00:57
 * @FilePath: \fancontrol32\src\main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#include <Arduino.h>
#include <ArduinoEigenDense.h> // Eigen库用于矩阵操作

HardwareSerial serial1(PA10, PA9); // 串口1
// HardwareSerial serial2(PA3, PA2); // 串口2
// HardwareSerial serial3(PB11, PB10); // 串口3

int adcPin = A0;   // A0 对应 PA0
int adcValue = 0;


void setup()
{
  serial1.begin(115200);
  // serial2.begin(115200);
  // serial3.begin(115200);
}

void loop()
{
  adcValue = analogRead(adcPin);
  serial1.println(adcValue);
  // serial1.println("1 Hello World!");
  // serial2.println("2 Hello World!");
  // serial3.println("3 Hello World!");
  delay(1000);
}