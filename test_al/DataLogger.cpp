#include "DataLogger.h"
using namespace std;

// 获取当前时间的字符串表示
string DataLogger::getCurrentTimeString() {
    time_t now = time(0);
    struct tm timeinfo;
    char buffer[80];
    localtime_s(&timeinfo, &now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return string(buffer);
}

// 构造函数：初始化数据记录器，创建日志文件
DataLogger::DataLogger() {
    // 获取当前时间戳作为文件名
    time_t now = time(0);
    unsigned long long timestamp = static_cast<unsigned long long>(now);
    string timestampStr = to_string(timestamp);

    // 创建并打开数据文件
    string dataFileName = "data_" + timestampStr + ".csv";
    dataFile.open(dataFileName, ios::out);
    if (!dataFile.is_open()) {
        throw runtime_error("Failed to open data file: " + dataFileName);
    }

    // 创建并打开警告日志文件
    string logFileName = "log_" + timestampStr + ".txt";
    logFile.open(logFileName, ios::out);
    if (!logFile.is_open()) {
        throw runtime_error("Failed to open log file: " + logFileName);
    }

    // 写入CSV文件头
    dataFile << "Time(s),N1(%),EGT(°),Fuel Flow(kg/h),Fuel Amount(kg)\n";
    dataFile.flush();

    // 写入日志文件头
    logFile << "Engine Warning Log - Started at: "
        << getCurrentTimeString() << "\n"
        << "----------------------------------------\n";
    logFile.flush();
}

// 析构函数：确保文件正确关闭
DataLogger::~DataLogger() {
    if (dataFile.is_open()) {
        dataFile.close();
    }
    if (logFile.is_open()) {
        logFile.close();
    }
}

// 记录发动机运行数据
void DataLogger::logData(double time, double n1, double egt, double fuelFlow, double fuelAmount) {
    if (dataFile.is_open()) {
        dataFile << fixed << setprecision(2)
            << time << ","          // 时间（秒）
            << n1 << ","            // N1转速（百分比）
            << egt << ","           // 排气温度（摄氏度）
            << fuelFlow << ","      // 燃油流量（kg/h）
            << fuelAmount << "\n";  // 剩余燃油量（kg）
        dataFile.flush();
    }
}

// 记录警告信息
void DataLogger::logWarning(double time, const string& warning) {
    if (logFile.is_open()) {
        logFile << fixed << setprecision(2)
            << "[" << time << "s] " // 记录警告发生的时间
            << warning << "\n";     // 警告信息
        logFile.flush();
    }
}