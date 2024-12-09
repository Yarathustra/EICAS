#pragma once
#include <fstream>
#include <string>
#include <iomanip>
#include <ctime>

class DataLogger {
private:
    std::ofstream dataFile;
    std::ofstream logFile;
    std::string getCurrentTimeString();

public:
    DataLogger();
    ~DataLogger();
    void logData(double time, double n1, double egt, double fuelFlow, double fuelAmount);
    void logWarning(double time, const std::string& warning);
}; 