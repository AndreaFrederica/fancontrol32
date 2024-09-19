/*
 * @Author: AndreaFrederica andreafrederica@outlook.com
 * @Date: 2024-09-19 18:46:03
 * @LastEditors: AndreaFrederica andreafrederica@outlook.com
 * @LastEditTime: 2024-09-19 20:10:06
 * @FilePath: \fancontrol32\src\main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "SdFat.h"
#include "sdios.h"
#include <Arduino.h>
#include <ArduinoEigenDense.h> // Eigen库用于矩阵操作
#include <SPI.h>

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

// 默认的电压区间和 PWM 占比
const float DEFAULT_VOLTAGE_START = 7.0;
const float DEFAULT_VOLTAGE_END = 13.8;
const float DEFAULT_PWM_START = 50.0;
const float DEFAULT_PWM_END = 99.0;

// 创建默认 CSV 文件
void createDefaultCSV() {
  file = sd.open(CSV_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("Error: Unable to create CSV file.");
    return;
  }

  // 写入标题行
  file.println("Voltage,FanPercent,PWM");

  // 生成默认数据并写入 CSV 文件
  const int numEntries = 10; // 默认数据点数量
  float voltageStep =
      (DEFAULT_VOLTAGE_END - DEFAULT_VOLTAGE_START) / (numEntries - 1);
  float pwmStep = (DEFAULT_PWM_END - DEFAULT_PWM_START) / (numEntries - 1);

  for (int i = 0; i < numEntries; i++) {
    float voltage = DEFAULT_VOLTAGE_START + i * voltageStep;
    float pwm = DEFAULT_PWM_START + i * pwmStep;
    // 假设风扇百分比与电压成线性关系
    float fanPercent =
        map(voltage, DEFAULT_VOLTAGE_START, DEFAULT_VOLTAGE_END, 0, 100);

    // 写入一行数据
    file.print(voltage, 1);
    file.print(", ");
    file.print(fanPercent, 1);
    file.print(", ");
    file.println(pwm, 1);
  }

  file.close();
  Serial.println("Default CSV file created with default data.");
}

// 检查并创建默认 CSV 文件
void checkAndCreateCSV() {
  if (!sd.begin(SD_CS, SPI_SPEED)) {
    Serial.println("SD initialization failed.");
    return;
  }

  if (!sd.exists(CSV_FILE)) {
    Serial.println("CSV file not found. Creating default CSV file...");
    createDefaultCSV();
  } else {
    Serial.println("CSV file already exists.");
  }
}

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
  Serial.begin(9600);

  cout << F("\nInitializing SD card...\n");
  if (!sd.begin(SD_CS, SPI_SPEED)) {
    cout << F("SD initialization failed.\n");
    return;
  }

  // 列出所有文件
  listFiles();

  // 查找 CSV_FILE
  // if (!findFanControl()) {
  //   cout << F("Failed to read CSV_FILE\n");
  // } else {
  //   cout << F("Finished parsing CSV_FILE\n");
  // }
  checkAndCreateCSV();
  // 读取CSV文件
  if (!readCSVFile(CSV_FILE)) {
    cout << F("Failed to read CSV_FILE\n");
    return;
  }
  // 输出读取到的CSV数据
  Serial.println("电压, 风扇百分比, PWM占空比");
  for (int i = 0; i < dataCount; i++) {
    Serial.print(voltageData[i]);
    Serial.print(", ");
    Serial.print(fanPercentData[i]);
    Serial.print(", ");
    Serial.println(pwmPercentData[i]);
  }

  // 拟合电压-风扇百分比曲线
  fitCurve(voltageData, fanPercentData, dataCount, voltageToFanPercentCoeffs);

  // 拟合风扇百分比-PWM曲线
  fitCurve(fanPercentData, pwmPercentData, dataCount, fanPercentToPWMCoeffs);

  // 输出拟合的系数
  Serial.print("电压-风扇百分比系数: ");
  Serial.print(voltageToFanPercentCoeffs[0]);
  Serial.print(", ");
  Serial.print(voltageToFanPercentCoeffs[1]);
  Serial.print(", ");
  Serial.println(voltageToFanPercentCoeffs[2]);

  Serial.print("风扇百分比-PWM系数: ");
  Serial.print(fanPercentToPWMCoeffs[0]);
  Serial.print(", ");
  Serial.print(fanPercentToPWMCoeffs[1]);
  Serial.print(", ");
  Serial.println(fanPercentToPWMCoeffs[2]);
}

void loop() {
  // 通过拟合函数计算并输出结果
  float inputVoltage = 5.0; // 示例电压
  float fanPercent = calculateY(inputVoltage, voltageToFanPercentCoeffs);
  float pwmDuty = calculateY(fanPercent, fanPercentToPWMCoeffs);

  Serial.print("输入电压: ");
  Serial.print(inputVoltage);
  Serial.print("V, 对应风扇百分比: ");
  Serial.print(fanPercent);
  Serial.print("%, 对应PWM占空比: ");
  Serial.println(pwmDuty);

  delay(1000); // 每秒更新一次
}
