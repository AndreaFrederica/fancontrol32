/*
 * @Author: AndreaFrederica andreafrederica@outlook.com
 * @Date: 2024-09-19 18:46:03
 * @LastEditors: AndreaFrederica andreafrederica@outlook.com
 * @LastEditTime: 2024-09-19 19:37:27
 * @FilePath: \fancontrol32\src\main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <Arduino.h>
#include <ArduinoEigenDense.h> // Eigen库用于矩阵操作
#include <SPI.h>
#include "SdFat.h"
#include "sdios.h"

#define SD_CS PA7

// 存储CSV数据
float voltageData[10]; // 假设最多10个数据点
float fanPercentData[10];
float pwmPercentData[10];
int dataCount = 0; // 实际数据点数

// 曲线拟合
float voltageToFanPercentCoeffs[3]; // 电压-风扇百分比拟合系数
float fanPercentToPWMCoeffs[3];     // 风扇百分比-PWM占空比拟合系数

// const String csv_file = "fancontrol.csv";
#define CSV_FILE "fancontrol.csv"

// 定义文件系统类型和SPI速度
#define SD_FAT_TYPE 3
#define SPI_SPEED SD_SCK_MHZ(4)

// 文件系统对象
#if SD_FAT_TYPE == 0
SdFat sd;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif // SD_FAT_TYPE

// 串口流
ArduinoOutStream cout(Serial);

// 文件查找函数
void listFiles() {
  cout << F("Listing all files:\n");
  sd.ls(LS_R | LS_DATE | LS_SIZE); // 列出所有文件
}

bool findFanControl() {
  if (!file.open(CSV_FILE, FILE_READ)) {
    cout << F("Error: CSV_FILE not found.\n");
    return false;
  }
  return true;
}

// 曲线拟合函数，使用二次多项式拟合 y = a*x^2 + b*x + c
void fitCurve(float xData[], float yData[], int count, float coeffs[]) {
  Eigen::MatrixXf A(count, 3);
  Eigen::VectorXf b(count);

  for (int i = 0; i < count; i++) {
    A(i, 0) = xData[i] * xData[i]; // x^2
    A(i, 1) = xData[i];            // x
    A(i, 2) = 1;                   // 常数项
    b(i) = yData[i];               // y
  }

  // 计算系数
  Eigen::VectorXf result = A.colPivHouseholderQr().solve(b);

  // 将结果赋给系数数组
  coeffs[0] = result(0);
  coeffs[1] = result(1);
  coeffs[2] = result(2);
}

// 通过拟合函数计算y值
float calculateY(float x, float coeffs[]) {
  return coeffs[0] * x * x + coeffs[1] * x + coeffs[2];
}

// 读取并解析CSV文件
bool readCSVFile(const char *filename) {
  file.open(filename, FILE_READ);

  if (!file) {
    Serial.println("无法打开文件");
    return false;
  }

  int index = 0;
  while (file.available() && index < 10) {
    String line = file.readStringUntil('\n'); // 读取每一行
    int commaIndex1 = line.indexOf(',');      // 寻找第一个逗号
    int commaIndex2 = line.indexOf(',', commaIndex1 + 1); // 第二个逗号

    if (commaIndex1 > 0 && commaIndex2 > 0) {
      voltageData[index] = line.substring(0, commaIndex1).toFloat();
      fanPercentData[index] =
          line.substring(commaIndex1 + 1, commaIndex2).toFloat();
      pwmPercentData[index] = line.substring(commaIndex2 + 1).toFloat();
      index++;
    }
  }

  dataCount = index;
  file.close();
  return true;
}

void setup() {
  // Serial.begin(9600);

  // // 等待 USB 串口连接
  // while (!Serial) {
  //   yield();
  // }
  Serial1.begin(9600);

  cout << F("\nInitializing SD card...\n");
  if (!sd.begin(SD_CS,SPI_SPEED)) {
    cout << F("SD initialization failed.\n");
    return;
  }

  // 列出所有文件
  listFiles();

  // 查找 CSV_FILE
  if (!findFanControl()) {
    cout << F("Failed to read CSV_FILE\n");
  } else {
    cout << F("Finished parsing CSV_FILE\n");
  }
  // 读取CSV文件
  if (!readCSVFile(CSV_FILE)) {
    cout << F("Failed to read CSV_FILE\n");
    return;
  }
  // 输出读取到的CSV数据
  Serial1.println("电压, 风扇百分比, PWM占空比");
  for (int i = 0; i < dataCount; i++) {
    Serial1.print(voltageData[i]);
    Serial1.print(", ");
    Serial1.print(fanPercentData[i]);
    Serial1.print(", ");
    Serial1.println(pwmPercentData[i]);
  }

  // 拟合电压-风扇百分比曲线
  fitCurve(voltageData, fanPercentData, dataCount, voltageToFanPercentCoeffs);

  // 拟合风扇百分比-PWM曲线
  fitCurve(fanPercentData, pwmPercentData, dataCount, fanPercentToPWMCoeffs);

  // 输出拟合的系数
  Serial1.print("电压-风扇百分比系数: ");
  Serial1.print(voltageToFanPercentCoeffs[0]);
  Serial1.print(", ");
  Serial1.print(voltageToFanPercentCoeffs[1]);
  Serial1.print(", ");
  Serial1.println(voltageToFanPercentCoeffs[2]);

  Serial1.print("风扇百分比-PWM系数: ");
  Serial1.print(fanPercentToPWMCoeffs[0]);
  Serial1.print(", ");
  Serial1.print(fanPercentToPWMCoeffs[1]);
  Serial1.print(", ");
  Serial1.println(fanPercentToPWMCoeffs[2]);
}

void loop() {
  // 通过拟合函数计算并输出结果
  float inputVoltage = 5.0; // 示例电压
  float fanPercent = calculateY(inputVoltage, voltageToFanPercentCoeffs);
  float pwmDuty = calculateY(fanPercent, fanPercentToPWMCoeffs);

  Serial1.print("输入电压: ");
  Serial1.print(inputVoltage);
  Serial1.print("V, 对应风扇百分比: ");
  Serial1.print(fanPercent);
  Serial1.print("%, 对应PWM占空比: ");
  Serial1.println(pwmDuty);

  delay(1000); // 每秒更新一次
}
