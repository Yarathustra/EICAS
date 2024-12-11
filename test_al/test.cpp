// test.cpp,请谨慎改动GUI类和Engine类
#include <iostream>
#include <windows.h>
#include <graphics.h>
#include <conio.h>
#include <math.h>
#include <fstream>
#include <ctime>
#include <string>
#include <vector>
#include <iomanip>  
#include <map>
#include "DataLogger.h"  
#include "WarningTypes.h"  
#include "Sensor.h"  
using namespace std;

#define PI 3.14159265358979323846


// 确保 Engine 正确使用 DataLogger
class Engine {
private:
    Sensor* speedSensorL1;  // 左发N1传感器1
    Sensor* speedSensorL2;  // 左发N1传感器2
    Sensor* speedSensorR1;  // 右发N1传感器1
    Sensor* speedSensorR2;  // 右发N1传感器2
    Sensor* egtSensorL1;  // 左发EGT传感器1
    Sensor* egtSensorL2;  // 左发EGT传感器2
    Sensor* egtSensorR1;  // 右发EGT传感器1
    Sensor* egtSensorR2;  // 右发EGT传感器2
    Sensor* fuelSensor;  // 燃油传感器
    double fuelFlow;
    double fuelAmount;
    bool isRunning;
    bool isStarting;
    bool isStopping;  // 停止状态
    bool isThrusting;  // 推力状态
    double stopStartTime;  // 开始停止时间
    static const double RATED_SPEED; // 额定转速
    static const double T0;  // 初始温度
    static const double STOP_DURATION;  // 停止持续时间，10秒

    // 机械参数
    double N1;        // 转速
    double temperature; // 温度
    double prevFuelAmount; // 上一次燃油量
    double timeStep;      // 时间步长
    vector<WarningMessage> warnings;
    double lastWarningTime;
    double accumulatedTime;  // 累积时间
    const double THRUST_FUEL_STEP = 1.0;  // 每次推力增加的燃油量
    const double THRUST_PARAM_MIN_CHANGE = 0.03;  // 最小推力变化 3%
    const double THRUST_PARAM_MAX_CHANGE = 0.05;  // 最大推力变化 5%
    DataLogger* logger;  // 日志记录器指针
    map<string, double> lastLogTimes;  // 每次日志记录时间
    map<string, double> lastWarningTimes;  // 添加警告记录时间映射

    void addWarning(const string& message, WarningLevel level, double currentTime) {
        // 检查是否在5秒内已经记录过相同的警告
        auto it = lastWarningTimes.find(message);
        if (it != lastWarningTimes.end()) {
            if (currentTime - it->second < 5.0) {
                // 如果距离上次记录不到5秒，则不重复记录
                return;
            }
        }

        // 更新该警告的最后记录时间
        lastWarningTimes[message] = currentTime;

        // 添加警告到列表
        warnings.push_back(WarningMessage(message, level, currentTime));

        // 记录到日志文件
        if (logger) {
            logger->logWarning(currentTime, message);
        }
    }
public:
    // Engine
    Sensor* getSpeedSensorL1() { return speedSensorL1; }
    Sensor* getSpeedSensorL2() { return speedSensorL2; }
    Sensor* getSpeedSensorR1() { return speedSensorR1; }
    Sensor* getSpeedSensorR2() { return speedSensorR2; }
    Sensor* getEgtSensorL1() { return egtSensorL1; }
    Sensor* getEgtSensorL2() { return egtSensorL2; }
    Sensor* getEgtSensorR1() { return egtSensorR1; }
    Sensor* getEgtSensorR2() { return egtSensorR2; }
    double getN1() const { return N1; }
    double getTemperature() const { return temperature; }
    double getFuelFlow() const { return fuelFlow; }
    double getFuelAmount() const { return fuelAmount; }
    bool getIsRunning() const { return isRunning; }
    bool getIsStarting() const { return isStarting; }

    void stop() {
        isRunning = false;
        isStarting = false;
        isStopping = true;  // 停止状态
        stopStartTime = accumulatedTime;  // 记录停止开始时间
        fuelFlow = 0;  // 燃油流量归零
    }

    void updateFuelFlow() {
        if (fuelFlow > 0) {
            fuelAmount -= fuelFlow * timeStep;
            if (fuelAmount <= 0) {
                fuelAmount = 0;
                // 燃油耗尽，自动停止发动机
                stop();
                // 添加燃油耗尽警告
                addWarning("Fuel Exhausted - Engine Shutdown", WARNING, getCurrentTime());
            }

            // 更新燃油传感器的值
            fuelSensor->setValue(fuelAmount);
        }
    }
    Engine() {
        speedSensorL1 = new Sensor();
        speedSensorL2 = new Sensor();
        speedSensorR1 = new Sensor();
        speedSensorR2 = new Sensor();
        egtSensorL1 = new Sensor();
        egtSensorL2 = new Sensor();
        egtSensorR1 = new Sensor();
        egtSensorR2 = new Sensor();
        fuelSensor = new Sensor();  // 初始化燃油传感器
        fuelFlow = 0;
        fuelAmount = 20000;
        isRunning = false;
        isStarting = false;
        lastWarningTime = 0;

        // 初始化机械参数
        N1 = 0;
        temperature = T0;  // 初始温度
        prevFuelAmount = fuelAmount;
        timeStep = 0.005;  // 5ms
        accumulatedTime = 0.0;  // 初始化累计时间
        isStopping = false;
        stopStartTime = 0;
        isThrusting = false;  // 初始化推力状态

        // 初始化EGT传感器
        egtSensorL1->setValue(T0);
        egtSensorL2->setValue(T0);
        egtSensorR1->setValue(T0);
        egtSensorR2->setValue(T0);
        logger = new DataLogger();  // 初始化日志记录器
        lastWarningTimes.clear();  // 确保警告时间映射为空
    }

    void updateStartPhase(double timeStep) {
        isThrusting = false;
        // 累计时间
        accumulatedTime += timeStep;

        // 输出时间步长和累计时间
        std::cout << "Time step: " << timeStep << ", Accumulated time: " << accumulatedTime << std::endl;

        if (accumulatedTime <= 2.0) {
            // 第一阶段
            double speed = 10000.0 * accumulatedTime;
            N1 = (speed / RATED_SPEED) * 100.0;

            speedSensorL1->setValue(speed);
            speedSensorL2->setValue(speed);
            speedSensorR1->setValue(speed);
            speedSensorR2->setValue(speed);
            fuelFlow = 5.0 * accumulatedTime;
            temperature = T0;  // 初始温度

            egtSensorL1->setValue(temperature);
            egtSensorL2->setValue(temperature);
            egtSensorR1->setValue(temperature);
            egtSensorR2->setValue(temperature);

            isStarting = true;
        }
        else {
            // 第二阶段
            double t = accumulatedTime - 2.0;  // t0开始时间
            double speed = 20000.0 + 23000.0 * log10(1.0 + t);
            N1 = (speed / RATED_SPEED) * 100.0;

            speedSensorL1->setValue(speed);
            speedSensorL2->setValue(speed);
            speedSensorR1->setValue(speed);
            speedSensorR2->setValue(speed);
            fuelFlow = 10.0 + 42.0 * log10(1.0 + t);

            // 计算温度 T = 900*lg(t-1) + T0
            temperature = 900.0 * log10(t + 1.0) + T0;

            double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;
            double displaySpeed = speed * randFactor;

            speedSensorL1->setValue(displaySpeed);
            speedSensorL2->setValue(displaySpeed);
            speedSensorR1->setValue(displaySpeed);
            speedSensorR2->setValue(displaySpeed);
            temperature *= randFactor;

            egtSensorL1->setValue(temperature);
            egtSensorL2->setValue(temperature);
            egtSensorR1->setValue(temperature);
            egtSensorR2->setValue(temperature);

            // N1达到95%转速
            if (N1 >= 95.0) {
                isStarting = false;
                isRunning = true;
                std::cout << "Reached running state" << std::endl;
            }
        }

        updateFuelFlow();
    }

    void start() {
        isRunning = false;  // 设置为false
        isStarting = true;
        isStopping = false;  // 停止状态为false
        accumulatedTime = 0.0;  // 重置累计时间
    }

    void update(double timeStep) {
        accumulatedTime += timeStep;

        if (isStarting) {
            updateStartPhase(timeStep);
        }
        else if (isRunning) {
            // 燃油流量更新
            if (isThrusting) {
                updateNewRunningPhase(timeStep);
            }
            else {
                updateRunningPhase(timeStep);
            }
        }
        else if (isStopping) {
            updateStopPhase(timeStep);
        }
    }

    void updateRunningPhase(double timeStep) {
        // 正常运行阶段，N1保持在95%
        double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;  // 0.97到1.03之间的随机因子

        // 转速
        double baseSpeed = RATED_SPEED * 0.95;  // 95%的额定转速
        double speed = baseSpeed * randFactor;
        N1 = (speed / RATED_SPEED) * 100.0;

        // 更新传感器值
        speedSensorL1->setValue(speed);
        speedSensorL2->setValue(speed);
        speedSensorR1->setValue(speed);
        speedSensorR2->setValue(speed);

        // 温度 - 目标温度为850度
        double targetTemp = 850.0;  // 目标温度
        double tempDiff = targetTemp - temperature;
        double tempAdjustRate = 0.1;  // 温度调整速率

        // 更新温度
        temperature += tempDiff * tempAdjustRate;
        // 随机因子调整温度
        temperature *= randFactor;

        egtSensorL1->setValue(temperature);
        egtSensorL2->setValue(temperature);
        egtSensorR1->setValue(temperature);
        egtSensorR2->setValue(temperature);

        // 燃油流量
        if (fuelFlow > 0) {
            fuelFlow = 40 * randFactor;  // 40的基础燃油流量
            updateFuelFlow();
        }
    }

    void updateNewRunningPhase(double timeStep) {

        // 推力状态下，N1变化在1%到3%之间
        double randFactor = 1.0 + (rand() % 4 - 2) / 100.0;

        // 更新转速
        double speed = (N1 / 100.0) * RATED_SPEED * randFactor;
        speedSensorL1->setValue(speed);
        speedSensorL2->setValue(speed);
        speedSensorR1->setValue(speed);
        speedSensorR2->setValue(speed);

        egtSensorL1->setValue(temperature * randFactor);
        egtSensorL2->setValue(temperature * randFactor);
        egtSensorR1->setValue(temperature * randFactor);
        egtSensorR2->setValue(temperature * randFactor);

        // 燃油流量
        if (fuelFlow > 0) {
            updateFuelFlow();
        }

        // 输出当前状态
        std::cout << "New Running Phase - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow
            << ", Time Step: " << timeStep << std::endl;
    }

    void updateStopPhase(double timeStep) {
        isThrusting = false;
        double elapsedStopTime = accumulatedTime - stopStartTime;

        if (elapsedStopTime >= STOP_DURATION) {
            // 停止完成
            isStopping = false;
            N1 = 0;
            temperature = T0;  // 恢复初始温度
            return;
        }

        // 指数衰减
        const double a = 0.9;  // 衰减系数

        // 计算衰减比例
        double decayRatio = pow(a, elapsedStopTime);

        // 更新转速和温度
        N1 = N1 * decayRatio;
        temperature = T0 + (temperature - T0) * decayRatio;

        // 随机因子调整显示值
        double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;
        double displaySpeed = (N1 / 100.0) * RATED_SPEED * randFactor;

        speedSensorL1->setValue(displaySpeed);
        speedSensorL2->setValue(displaySpeed);
        speedSensorR1->setValue(displaySpeed);
        speedSensorR2->setValue(displaySpeed);
        egtSensorL1->setValue(temperature);
        egtSensorL2->setValue(temperature);
        egtSensorR1->setValue(temperature);
        egtSensorR2->setValue(temperature);

        // 输出停止阶段状态
        std::cout << "Stop Phase - Time: " << elapsedStopTime
            << "s, N1: " << N1
            << "%, Temp: " << temperature << std::endl;
    }

    void checkWarnings(double currentTime) {
        // 检查传感器有效性
        bool n1L1Failed = !speedSensorL1->getValidity();
        bool n1L2Failed = !speedSensorL2->getValidity();
        bool n1R1Failed = !speedSensorR1->getValidity();
        bool n1R2Failed = !speedSensorR2->getValidity();
        bool egtL1Failed = !egtSensorL1->getValidity();
        bool egtL2Failed = !egtSensorL2->getValidity();
        bool egtR1Failed = !egtSensorR1->getValidity();
        bool egtR2Failed = !egtSensorR2->getValidity();

        // 检查N1传感器故障
        if (n1L1Failed && !n1L2Failed && !n1R1Failed && !n1R2Failed) {
            addWarning("Single N1 Sensor Failure", NORMAL, currentTime);
        }
        if (!n1L1Failed && n1L2Failed && !n1R1Failed && !n1R2Failed) {
            addWarning("Single N1 Sensor Failure", NORMAL, currentTime);
        }
        if (!n1L1Failed && !n1L2Failed && n1R1Failed && !n1R2Failed) {
            addWarning("Single N1 Sensor Failure", NORMAL, currentTime);
        }
        if (!n1L1Failed && !n1L2Failed && !n1R1Failed && n1R2Failed) {
            addWarning("Single N1 Sensor Failure", NORMAL, currentTime);
        }
        if (n1L1Failed && n1L2Failed && !n1R1Failed && !n1R2Failed) {
            addWarning("Engine N1 Sensor Failure", CAUTION, currentTime);
        }
        if (!n1L1Failed && !n1L2Failed && n1R1Failed && !n1R2Failed) {
            addWarning("Engine N1 Sensor Failure", CAUTION, currentTime);
        }
        if (!n1L1Failed && !n1L2Failed && !n1R1Failed && n1R2Failed) {
            addWarning("Engine N1 Sensor Failure", CAUTION, currentTime);
        }

        // 检查EGT传感器故障
        if (egtL1Failed && !egtL2Failed && !egtR1Failed && !egtR2Failed) {
            addWarning("Single EGT Sensor Failure", NORMAL, currentTime);
        }
        if (!egtL1Failed && egtL2Failed && !egtR1Failed && !egtR2Failed) {
            addWarning("Single EGT Sensor Failure", NORMAL, currentTime);
        }
        if (!egtL1Failed && !egtL2Failed && egtR1Failed && !egtR2Failed) {
            addWarning("Single EGT Sensor Failure", NORMAL, currentTime);
        }
        if (!egtL1Failed && !egtL2Failed && !egtR1Failed && egtR2Failed) {
            addWarning("Single EGT Sensor Failure", NORMAL, currentTime);
        }
        if (egtL1Failed && egtL2Failed && !egtR1Failed && !egtR2Failed) {
            addWarning("Engine EGT Sensor Failure", CAUTION, currentTime);
        }
        if (!egtL1Failed && !egtL2Failed && egtR1Failed && !egtR2Failed) {
            addWarning("Engine EGT Sensor Failure", CAUTION, currentTime);
        }
        if (!egtL1Failed && !egtL2Failed && !egtR1Failed && egtR2Failed) {
            addWarning("Engine EGT Sensor Failure", CAUTION, currentTime);
        }

        // 双发动机传感器故障检查
        if ((n1L1Failed && n1L2Failed && !n1R1Failed && !n1R2Failed) && (egtL1Failed && egtL2Failed && !egtR1Failed && !egtR2Failed)) {
            addWarning("Dual Engine Sensor Failure - Shutdown", WARNING, currentTime);
            stop();
        }

        // 燃油系统警告
        if (fuelAmount < 1000 && isRunning) {
            addWarning("Low Fuel Level", CAUTION, currentTime);
        }

        if (!fuelSensor->getValidity()) {
            addWarning("Fuel Sensor Failure", WARNING, currentTime);
        }

        if (fuelFlow > 50) {
            addWarning("Fuel Flow Exceeded Limit", CAUTION, currentTime);
        }

        // 转速警告
        double n1Percent = getN1();
        if (n1Percent > 120) {
            addWarning("N1 Overspeed Level 2 - Engine Shutdown", WARNING, currentTime);
            stop();
        }
        else if (n1Percent > 105) {
            addWarning("N1 Overspeed Level 1", CAUTION, currentTime);
        }

        // 温度警告
        if (isStarting) {
            if (temperature > 1000) {
                addWarning("EGT Overtemp Level 2 During Start - Shutdown", WARNING, currentTime);
                stop();
            }
            else if (temperature > 850) {
                addWarning("EGT Overtemp Level 1 During Start", CAUTION, currentTime);
            }
        }
        else if (isRunning) {
            if (temperature > 1100) {
                addWarning("EGT Overtemp Level 2 During Run - Shutdown", WARNING, currentTime);
                stop();
            }
            else if (temperature > 950) {
                addWarning("EGT Overtemp Level 1 During Run", CAUTION, currentTime);
            }
        }
    }

    const vector<WarningMessage>& getWarnings() const {
        return warnings;
    }

    void increaseThrust() {
        if (!isRunning || isStarting || isStopping) {
            cout << "Not in running state" << endl;
            return;  // 只在运行状态下增加推力
        }
        isThrusting = true;

        // 增加燃油流量
        fuelFlow += THRUST_FUEL_STEP;

        // 随机增加3%~5%之间的推力
        double changeRatio = THRUST_PARAM_MIN_CHANGE +
            (static_cast<double>(rand()) / RAND_MAX) *
            (THRUST_PARAM_MAX_CHANGE - THRUST_PARAM_MIN_CHANGE);

        // 更新转速
        double baseSpeed = RATED_SPEED * N1 / 100.0;  // 基准转速
        double newSpeed = baseSpeed * (1.0 + changeRatio);
        N1 = (newSpeed / RATED_SPEED) * 100.0;

        // 限制N1最大值为125%
        if (N1 > 125.0) {
            N1 = 125.0;
            newSpeed = RATED_SPEED;
        }

        // 更新传感器显示值
        speedSensorL1->setValue(newSpeed);
        speedSensorL2->setValue(newSpeed);
        speedSensorR1->setValue(newSpeed);
        speedSensorR2->setValue(newSpeed);

        // 更新温度
        temperature = temperature * (1.0 + changeRatio);

        // 更新EGT传感器值
        egtSensorL1->setValue(temperature);
        egtSensorL2->setValue(temperature);
        egtSensorR1->setValue(temperature);
        egtSensorR2->setValue(temperature);

        // 输出当前状态
        std::cout << "Thrust increased - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow << std::endl;
    }

    void decreaseThrust() {
        if (!isRunning || isStarting || isStopping) {
            return;  // 只在运行状态下减少推力
        }
        isThrusting = true;

        // 保燃油量不会低于最小值
        if (fuelFlow > THRUST_FUEL_STEP) {
            fuelFlow -= THRUST_FUEL_STEP;
        }

        // 随机减少3%~5%之间的推力
        double changeRatio = THRUST_PARAM_MIN_CHANGE +
            (static_cast<double>(rand()) / RAND_MAX) *
            (THRUST_PARAM_MAX_CHANGE - THRUST_PARAM_MIN_CHANGE);

        // 更新转速
        double baseSpeed = RATED_SPEED * N1 / 100.0;  // 基准转速
        double newSpeed = baseSpeed * (1.0 - changeRatio);
        N1 = (newSpeed / RATED_SPEED) * 100.0;

        // 限制N1最小值为0%
        if (N1 < 0) {
            N1 = 0;
            newSpeed = 0;
        }

        // 更新传感器显示值
        speedSensorL1->setValue(newSpeed);
        speedSensorL2->setValue(newSpeed);
        speedSensorR1->setValue(newSpeed);
        speedSensorR2->setValue(newSpeed);

        // 更新温度
        temperature = temperature * (1.0 - changeRatio);

        // 更新EGT传感器值
        egtSensorL1->setValue(temperature);
        egtSensorL2->setValue(temperature);
        egtSensorR1->setValue(temperature);
        egtSensorR2->setValue(temperature);

        // 输出当前状态
        std::cout << "Thrust decreased - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow << std::endl;
    }

    // 模传感器故障的方法
    void simulateSensorFailure(int sensorType) {
        switch (sensorType) {
        case 0: // 单个N1传感器故障
            speedSensorL1->setValidity(false);
            break;
        case 1: // 双N1传感器故障
            speedSensorL1->setValidity(false);
            speedSensorL2->setValidity(false);
            break;
        case 2: // 单个EGT传感器故障
            egtSensorL1->setValidity(false);
            break;
        case 3: // 双EGT传感器故障
            egtSensorL1->setValidity(false);
            egtSensorL2->setValidity(false);
            break;
            // ... 其他故障类型
        }
    }

    double getCurrentTime() const {
        return static_cast<double>(GetTickCount64()) / 1000.0;
    }

    // 获取燃油传感器的方法
    Sensor* getFuelSensor() { return fuelSensor; }

    // 析构函数，释放资源
    ~Engine() {
        delete speedSensorL1;
        delete speedSensorL2;
        delete speedSensorR1;
        delete speedSensorR2;
        delete egtSensorL1;
        delete egtSensorL2;
        delete egtSensorR1;
        delete egtSensorR2;
        delete fuelSensor;  // 删除燃油传感器
        if (logger) {
            delete logger;  // 安全删除日志记录器
            logger = nullptr;
        }
        lastWarningTimes.clear();
    }

    // 添加新的测试方法
    void setFuelAmount(double amount) { fuelAmount = amount; }
    void setFuelFlow(double flow) { fuelFlow = flow; }
    void setN1(double value) { N1 = value; }
    void setTemperature(double temp) { temperature = temp; }

    void resetSensors() {
        speedSensorL1->setValidity(true);
        speedSensorL2->setValidity(true);
        speedSensorR1->setValidity(true);
        speedSensorR2->setValidity(true);
        egtSensorL1->setValidity(true);
        egtSensorL2->setValidity(true);
        egtSensorR1->setValidity(true);
        egtSensorR2->setValidity(true);
        fuelSensor->setValidity(true);
    }

    // 在重置警告时清除时间记录
    void resetWarnings() {
        warnings.clear();
        lastWarningTimes.clear();
    }
    };

    const double Engine::RATED_SPEED = 40000.0;
    const double Engine::T0 = 20.0;
    const double Engine::STOP_DURATION = 10.0;  // 10秒停止时间

    // GUI类
    class GUI {
    private:
        static const int WINDOW_WIDTH = 1600;    // 窗口宽度
        static const int WINDOW_HEIGHT = 900;    // 窗口高度
        static const int DIAL_RADIUS = 60;       // 表盘半径
        static const int DIAL_SPACING_X = 180;   // 表盘水平间距
        static const int DIAL_SPACING_Y = 180;   // 表盘垂直间距
        static const int DIAL_START_X = 100;     // 表盘起始X坐标
        static const int DIAL_START_Y = 150;     // 表盘起始Y坐标
        static const int WARNING_BOX_WIDTH = 380;  // 警告框宽度
        static const int WARNING_BOX_HEIGHT = 30;  // 警告框高度
        static const int WARNING_START_X = 750;    // 警告框起始X坐标
        static const int WARNING_START_Y = 100;    // 警告框起始Y坐标
        static const int WARNING_SPACING = 5;      // 警告框间距
        static const int REFRESH_RATE = 200;       // 刷新率200Hz = 5ms
        static const int FRAME_TIME = 1000 / REFRESH_RATE;  // 5ms
        ULONGLONG lastFrameTime;
        IMAGE* backBuffer;
        Engine* engine;
        // 确保 engine 指针有效

    public:
        struct Button {
            int x, y, width, height;
            const TCHAR* text;
            bool pressed;
            COLORREF bgColor;
            COLORREF textColor;
        };

        Button startButton;
        Button stopButton;
        Button increaseThrust;
        Button decreaseThrust;

        // 添加测试警告按钮
        Button warningButtons[14];

        GUI(int w, int h, Engine* eng) : engine(eng) {
            // 确保 engine 指针有效
            if (!eng) {
                throw std::runtime_error("Invalid engine pointer");
            }

            // 初始化按钮
            startButton = { 50, 500, 120, 40, _T("START"), false, RGB(0, 150, 0), WHITE };
            stopButton = { 200, 500, 120, 40, _T("STOP"), false, RGB(150, 0, 0), WHITE };
            increaseThrust = { 350, 500, 120, 40, _T("THRUST+"), false, RGB(0, 100, 150), WHITE };
            decreaseThrust = { 500, 500, 120, 40, _T("THRUST-"), false, RGB(0, 100, 150), WHITE };

            // 初始化警告测试按钮
            const TCHAR* warningLabels[] = {
                    _T("Single N1"),      // 单个N1传感器故障
                    _T("Dual N1"),        // 双N1传感器故障
                    _T("Single EGT"),     // 单个EGT传感器故障
                    _T("Dual EGT"),       // 双EGT传感器故障
                    _T("Low Fuel"),       // 燃油量低
                    _T("Fuel Sensor"),    // 燃油传感器故障
                    _T("High Flow"),      // 燃油流量超限
                    _T("N1 Over L1"),     // N1超速一级
                    _T("N1 Over L2"),     // N1超速二级
                    _T("EGT Start L1"),   // 启动时EGT超温一级
                    _T("EGT Start L2"),   // 启动时EGT超温二级
                    _T("EGT Run L1"),     // 运行时EGT超温一级
                    _T("EGT Run L2"),     // 运行时EGT超温二级
                    _T("Reset All")   // 重置所有
            };

            // 设置按钮位置和样式
            int startX = 50;
            int startY = 600;
            int buttonWidth = 100;
            int buttonHeight = 30;
            int buttonSpacing = 10;
            int buttonsPerRow = 7;

            for (int i = 0; i < 14; i++) {
                int row = i / buttonsPerRow;
                int col = i % buttonsPerRow;
                warningButtons[i] = {
                        startX + col * (buttonWidth + buttonSpacing),
                        startY + row * (buttonHeight + buttonSpacing),
                        buttonWidth,
                        buttonHeight,
                        warningLabels[i],
                        false,
                        RGB(100, 100, 100),
                        WHITE
                };
            }

            // 初始化图形系统
            initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
            setbkcolor(RGB(40, 40, 40));
            cleardevice();

            // 创建后备缓冲区
            backBuffer = new IMAGE(WINDOW_WIDTH, WINDOW_HEIGHT);
            if (!backBuffer) {
                throw std::runtime_error("Failed to create back buffer");
            }

            SetWorkingImage(backBuffer);
            cleardevice();
            SetWorkingImage(NULL);

            // 预热图系统
            BeginBatchDraw();
            for (int i = 0; i < 5; i++) {
                cleardevice();
                drawDials();
                drawButton(startButton);
                drawButton(stopButton);
                drawButton(increaseThrust);
                drawButton(decreaseThrust);
                drawStatus(false, false);
                drawAllWarningBoxes();
            }
            EndBatchDraw();

            lastFrameTime = GetTickCount64();
        }

        ~GUI() {
            if (backBuffer) {
                delete backBuffer;
                backBuffer = nullptr;
            }
        }

        void drawArcDial(int x, int y, int radius, double value, double maxValue,
            const TCHAR* label, COLORREF color) {
            // 绘制表盘背景圆圈
            setcolor(RGB(60, 60, 60));
            setfillcolor(RGB(20, 20, 20));
            fillcircle(x, y, radius);

            // 判断是N1表盘还是EGT表盘
            bool isN1Dial = _tcsstr(label, _T("N1")) != nullptr;
            bool isEGTDial = _tcsstr(label, _T("EGT")) != nullptr;
            double valueToDisplay = value;
            double maxValueToDisplay = maxValue;
            bool isStarting = engine->getIsStarting();
            int arcDegrees = 360;  // 默认360度

            if (isN1Dial) {
                valueToDisplay = (value / 40000.0) * 100;  // 40000为额定转速
                maxValueToDisplay = 125;  // 最大125%
                arcDegrees = 210;  // N1表盘使用210度
            }
            else if (isEGTDial) {
                maxValueToDisplay = 1200;  // EGT最大值1200度
                arcDegrees = 210;   // EGT表盘也使用210度
            }

            // 刻度
            int startAngle = 90;  // 起始角度，12点位置

            // 绘制主刻度线
            for (int i = 0; i <= arcDegrees; i += 30) {
                double angle = (startAngle - i) * PI / 180;
                int x1 = x + (radius - 10) * cos(angle);
                int y1 = y - (radius - 10) * sin(angle);
                int x2 = x + radius * cos(angle);
                int y2 = y - radius * sin(angle);
                setcolor(RGB(100, 100, 100));
                setlinestyle(PS_SOLID, 2);
                line(x1, y1, x2, y2);
            }

            // 绘制次刻度线
            for (int i = 0; i <= arcDegrees; i += 6) {
                if (i % 30 != 0) {  // 跳过主刻度线位置
                    double angle = (startAngle - i) * PI / 180;
                    int x1 = x + (radius - 5) * cos(angle);
                    int y1 = y - (radius - 5) * sin(angle);
                    int x2 = x + radius * cos(angle);
                    int y2 = y - radius * sin(angle);
                    setcolor(RGB(60, 60, 60));
                    setlinestyle(PS_SOLID, 1);
                    line(x1, y1, x2, y2);
                }
            }

            // 根据值状态选择颜色
            COLORREF valueColor = RGB(255, 255, 255);  // 默认白色
            if (isN1Dial) {
                if (valueToDisplay > 105) valueColor = RGB(255, 128, 0);
                if (valueToDisplay > 120) valueColor = RGB(255, 0, 0);
            }
            else if (isEGTDial) {
                if (isStarting) {
                    // 启动阶段温度警告
                    if (valueToDisplay > 1000) {
                        valueColor = RGB(255, 0, 0);      // 红色警告（级别2）
                    }
                    else if (valueToDisplay > 850) {
                        valueColor = RGB(255, 128, 0);    // 橙色警告（级别1）
                    }
                }
                else {
                    // 运行阶段温度警告
                    if (valueToDisplay > 1100) {
                        valueColor = RGB(255, 0, 0);      // 红色警告（级别4）
                    }
                    else if (valueToDisplay > 950) {
                        valueColor = RGB(255, 128, 0);    // 橙色警告（级别3）
                    }
                }
            }

            // 绘制值的弧线
            double angleRatio = isN1Dial ? (valueToDisplay / maxValueToDisplay) : (valueToDisplay / maxValue);
            angleRatio = min(angleRatio, 1.0);
            double angle = angleRatio * arcDegrees;

            // 使用颜色填充弧线
            setfillcolor(valueColor);
            setcolor(valueColor);
            POINT* points = new POINT[362];
            points[0].x = x;
            points[0].y = y;

            for (int i = 0; i <= angle; i++) {
                double rad = (90 - i) * PI / 180;
                points[i + 1].x = x + (radius - 15) * cos(rad);  // 内圈半径减小以留出刻度空间
                points[i + 1].y = y - (radius - 15) * sin(rad);
            }

            solidpolygon(points, static_cast<int>(angle) + 2);
            delete[] points;

            // 绘制指针
            double pointerAngle = (90 - angle) * PI / 180;
            int pointerLength = radius - 10;
            int pointerX = x + pointerLength * cos(pointerAngle);
            int pointerY = y - pointerLength * sin(pointerAngle);

            // 绘制指针阴影
            setcolor(RGB(20, 20, 20));
            setlinestyle(PS_SOLID, 3);
            line(x, y, pointerX + 2, pointerY + 2);

            // 绘制指针本体
            setcolor(WHITE);
            setlinestyle(PS_SOLID, 2);
            line(x, y, pointerX, pointerY);

            // 绘制指针中心装饰
            setfillcolor(RGB(40, 40, 40));
            fillcircle(x, y, 8);
            setfillcolor(valueColor);
            fillcircle(x, y, 4);

            // 绘制标签和值
            settextstyle(20, 0, _T("Arial"));
            settextcolor(valueColor);
            TCHAR valueText[32];
            
            // 判断是否为N1表盘且对应的传感器无效
            bool isInvalidN1L1 = _tcscmp(label, _T("N1-L1")) == 0 && !engine->getSpeedSensorL1()->getValidity();
            bool isInvalidN1L2 = _tcscmp(label, _T("N1-L2")) == 0 && !engine->getSpeedSensorL2()->getValidity();
            bool isInvalidN1R1 = _tcscmp(label, _T("N1-R1")) == 0 && !engine->getSpeedSensorR1()->getValidity();
            bool isInvalidN1R2 = _tcscmp(label, _T("N1-R2")) == 0 && !engine->getSpeedSensorR2()->getValidity();

            // 判断是否为EGT表盘且对应的传感器无效
            bool isInvalidEGT1 = _tcscmp(label, _T("EGT-L1")) == 0 && !engine->getEgtSensorL1()->getValidity();
            bool isInvalidEGT2 = _tcscmp(label, _T("EGT-L2")) == 0 && !engine->getEgtSensorL2()->getValidity();
            bool isInvalidEGT3 = _tcscmp(label, _T("EGT-R1")) == 0 && !engine->getEgtSensorR1()->getValidity();
            bool isInvalidEGT4 = _tcscmp(label, _T("EGT-R2")) == 0 && !engine->getEgtSensorR2()->getValidity();

            if (isN1Dial && (isInvalidN1L1 || isInvalidN1L2 || isInvalidN1R1 || isInvalidN1R2)) {
                _stprintf_s(valueText, _T("%s: --"), label);
            } else if (isEGTDial && (isInvalidEGT1 || isInvalidEGT2 || isInvalidEGT3 || isInvalidEGT4)) {
                _stprintf_s(valueText, _T("%s: --"), label);
            } else if (isN1Dial) {
                _stprintf_s(valueText, _T("%s: %.1f%%"), label, valueToDisplay);
            } else {
                _stprintf_s(valueText, _T("%s: %.1f"), label, value);
            }
            outtextxy(x - 40, y + radius + 10, valueText);
        }

        void drawDials() {
            // 第一行 - 左侧传感器显示
            drawArcDial(DIAL_START_X, DIAL_START_Y, DIAL_RADIUS,
                engine->getSpeedSensorL1()->getValue(), 40000, _T("N1-L1"), RGB(0, 255, 0));
            drawArcDial(DIAL_START_X + DIAL_SPACING_X, DIAL_START_Y, DIAL_RADIUS,
                engine->getSpeedSensorL2()->getValue(), 40000, _T("N1-L2"), RGB(0, 255, 0));
            drawArcDial(DIAL_START_X + DIAL_SPACING_X * 2, DIAL_START_Y, DIAL_RADIUS,
                engine->getEgtSensorL1()->getValue(), 1200, _T("EGT-L1"), RGB(255, 128, 0));
            drawArcDial(DIAL_START_X + DIAL_SPACING_X * 3, DIAL_START_Y, DIAL_RADIUS,
                engine->getEgtSensorL2()->getValue(), 1200, _T("EGT-L2"), RGB(255, 128, 0));

            // 第二行 - 右侧传感器显示 (假设右侧传感器与左侧相同)
            drawArcDial(DIAL_START_X, DIAL_START_Y + DIAL_SPACING_Y, DIAL_RADIUS,
                engine->getSpeedSensorR1()->getValue(), 40000, _T("N1-R1"), RGB(0, 255, 0));
            drawArcDial(DIAL_START_X + DIAL_SPACING_X, DIAL_START_Y + DIAL_SPACING_Y, DIAL_RADIUS,
                engine->getSpeedSensorR2()->getValue(), 40000, _T("N1-R2"), RGB(0, 255, 0));
            drawArcDial(DIAL_START_X + DIAL_SPACING_X * 2, DIAL_START_Y + DIAL_SPACING_Y, DIAL_RADIUS,
                engine->getEgtSensorR1()->getValue(), 1200, _T("EGT-R1"), RGB(255, 128, 0));
            drawArcDial(DIAL_START_X + DIAL_SPACING_X * 3, DIAL_START_Y + DIAL_SPACING_Y, DIAL_RADIUS,
                engine->getEgtSensorR2()->getValue(), 1200, _T("EGT-R2"), RGB(255, 128, 0));
        }

        void drawButton(const Button& btn) {
            // 绘制按钮背景
            setfillcolor(btn.pressed ? RGB(150, 150, 150) : btn.bgColor);
            solidroundrect(btn.x, btn.y, btn.x + btn.width, btn.y + btn.height, 10, 10);

            // 绘制按钮文本
            settextstyle(20, 0, _T("Arial"));
            setbkmode(TRANSPARENT);
            settextcolor(btn.textColor);

            // 计算文本位置使其居中
            int textWidth = textwidth(btn.text);
            int textHeight = textheight(btn.text);
            int textX = btn.x + (btn.width - textWidth) / 2;
            int textY = btn.y + (btn.height - textHeight) / 2;

            outtextxy(textX, textY, btn.text);
        }

        void drawStatus(bool isStarting, bool isRunning) {
            settextstyle(24, 0, _T("Arial"));
            settextcolor(WHITE);
            if (!isStarting) {
                setfillcolor(RGB(0, 200, 0));
                solidroundrect(50, 20, 150, 50, 10, 10);
                settextcolor(BLACK);
                outtextxy(60, 25, _T("   start"));
            }

            if (!isRunning) {
                setfillcolor(RGB(0, 200, 0));
                solidroundrect(170, 20, 270, 50, 10, 10);
                settextcolor(BLACK);
                outtextxy(185, 25, _T("   run"));
            }
            if (isStarting) {
                setfillcolor(RGB(0, 255, 0));
                solidroundrect(50, 20, 150, 50, 10, 10);
                settextcolor(BLACK);
                outtextxy(60, 25, _T("   start"));
            }

            if (isRunning) {
                setfillcolor(RGB(0, 255, 0));
                solidroundrect(170, 20, 270, 50, 10, 10);
                settextcolor(BLACK);
                outtextxy(185, 25, _T("   run"));
            }
        }

        void drawWarningBox(const string& message, WarningLevel level, bool isActive, int x, int y) {
            COLORREF boxColor;
            switch (level) {
            case NORMAL: boxColor = WHITE; break;
            case CAUTION: boxColor = RGB(255, 128, 0); break;  // 橙色
            case WARNING: boxColor = RGB(255, 0, 0); break;    // 红色
            default: boxColor = RGB(128, 128, 128);            // 灰色
            }

            // 绘制警告框背景
            setfillcolor(RGB(30, 30, 30));
            solidrectangle(x - 2, y - 2, x + WARNING_BOX_WIDTH + 2, y + WARNING_BOX_HEIGHT + 2);

            // 绘制警告框
            setfillcolor(isActive ? RGB(60, 60, 60) : RGB(40, 40, 40));
            solidrectangle(x, y, x + WARNING_BOX_WIDTH, y + WARNING_BOX_HEIGHT);

            // 绘制警告框边框
            setcolor(isActive ? boxColor : RGB(80, 80, 80));
            rectangle(x - 1, y - 1, x + WARNING_BOX_WIDTH + 1, y + WARNING_BOX_HEIGHT + 1);

            // 绘制警告框文本
            settextstyle(16, 0, _T("Arial"));
            settextcolor(isActive ? boxColor : RGB(128, 128, 128));
            wstring wstr(message.begin(), message.end());
            outtextxy(x + 10, y + 5, wstr.c_str());

            // 绘制警告框左侧指示条
            if (isActive) {
                setfillcolor(boxColor);
                solidrectangle(x, y, x + 3, y + WARNING_BOX_HEIGHT);
            }
        }

        void drawAllWarningBoxes() {
            int x = WARNING_START_X;
            int y = WARNING_START_Y;
            const vector<WarningMessage>& warnings = engine->getWarnings();

            // 定义所有可能的警
            struct WarningDef {
                string message;
                WarningLevel level;
            };

            WarningDef allWarnings[] = {
                // 传感器故障
                {"Single N1 Sensor Failure", NORMAL},           // 单个N1传感器故障
                {"Engine N1 Sensor Failure", CAUTION},          // N1传感器故障
                {"Single EGT Sensor Failure", NORMAL},          // 单个EGT传感器故障
                {"Engine EGT Sensor Failure", CAUTION},         // 双EGT传感器故障
                {"Dual Engine Sensor Failure - Shutdown", WARNING},  // 双传感器故障

                // 燃油系统警告
                {"Low Fuel Level", CAUTION},                    // 燃油量低
                {"Fuel Sensor Failure", WARNING},               // 燃油传感器故障
                {"Fuel Flow Exceeded Limit", CAUTION},          // 燃油流量超限

                // 转速警告
                {"N1 Overspeed Level 1", CAUTION},             // N1超速一级
                {"N1 Overspeed Level 2 - Engine Shutdown", WARNING}, // N1超速二级

                // 温度警告
                {"EGT Overtemp Level 1 During Start", CAUTION},      // 启动时EGT超温一级
                {"EGT Overtemp Level 2 During Start - Shutdown", WARNING}, // 启动时EGT超温二级
                {"EGT Overtemp Level 1 During Run", CAUTION},        // 运行时EGT超温一级
                {"EGT Overtemp Level 2 During Run - Shutdown", WARNING}    // 运行时EGT超温二级
            };

            // 绘制所有警告框
            for (const auto& warningDef : allWarnings) {
                bool isActive = false;

                // 检查当前警告是否激活
                for (const auto& warning : warnings) {
                    // 检查警告是否在最近5秒内
                    if (warning.message == warningDef.message &&
                        warning.timestamp >= engine->getCurrentTime() - 5.0) {
                        isActive = true;
                        break;
                    }
                }

                drawWarningBox(warningDef.message, warningDef.level, isActive, x, y);
                y += WARNING_BOX_HEIGHT + WARNING_SPACING;

                // 每6个警告框换一列
                if (y > WARNING_START_Y + 6 * (WARNING_BOX_HEIGHT + WARNING_SPACING)) {
                    x += WARNING_BOX_WIDTH + WARNING_SPACING;
                    y = WARNING_START_Y;
                }
            }
        }

        void drawFuelFlow() {
            double fuelFlow = engine->getFuelFlow();

            // 设置燃油流量显示的颜色
            COLORREF textColor, borderColor, bgColor;
            if (fuelFlow > 0) {
                if (fuelFlow > 50) {
                    // 燃油流量超限 - 橙色警告
                    textColor = RGB(255, 128, 0);   // 橙色文本
                    borderColor = RGB(255, 128, 0); // 橙色边框
                    bgColor = RGB(40, 40, 40);      // 背景色
                }
                else {
                    // 正常燃油流量状态 - 白色
                    textColor = WHITE;              // 白色文本
                    borderColor = RGB(100, 100, 100); // 灰色边框
                    bgColor = RGB(40, 40, 40);       // 背景色
                }
            }
            else {
                // 燃油流量为零状态 - 灰色
                textColor = RGB(150, 150, 150);   // 灰色文本
                borderColor = RGB(100, 100, 100); // 灰色边框
                bgColor = RGB(40, 40, 40);       // 背景色
            }

            // 设置文本样式
            settextstyle(24, 0, _T("Arial"));
            settextcolor(textColor);

            // 格式化显示文本
            TCHAR flowText[64];
            _stprintf_s(flowText, _T("Fuel Flow: %.1f L/s"), fuelFlow);

            // 设置显示位置
            int x = 50;
            int y = 560;
            int width = 200;
            int height = 30;

            // 绘制背景框
            setfillcolor(RGB(30, 30, 30));
            solidrectangle(x - 1, y - 1, x + width + 1, y + height + 1);

            // 绘制背景色
            setfillcolor(bgColor);
            solidrectangle(x, y, x + width, y + height);

            // 绘制边框
            setcolor(borderColor);
            rectangle(x, y, x + width, y + height);

            // 绘制左侧指示条
            if (fuelFlow > 0) {
                setfillcolor(borderColor);
                solidrectangle(x, y, x + 3, y + height);
            }

            // 绘制文本，微调位置使其居中
            outtextxy(x + 10, y + (height - textheight(flowText)) / 2, flowText);
        }

        void drawFuelAmount() {
            double fuelAmount = engine->getFuelAmount();
            bool isFuelSensorValid = engine->getFuelSensor()->getValidity();

            // 设置油量显示的颜色
            COLORREF textColor;
            if (!isFuelSensorValid) {
                textColor = RGB(0, 255, 0);  // 传感器失效时显示绿色
            } else if (fuelAmount <= 0) {
                textColor = RGB(255, 0, 0);  // 红色
            } else if (fuelAmount < 1000) {
                textColor = RGB(255, 191, 0);  // 琥珀色
            } else {
                textColor = WHITE;  // 白色
            }

            // 设置文本样式
            settextstyle(24, 0, _T("Arial"));
            settextcolor(textColor);

            // 格式化显示文本
            TCHAR amountText[64];
            if (!isFuelSensorValid) {
                _stprintf_s(amountText, _T("Fuel Amount: -- L"));
            } else {
                _stprintf_s(amountText, _T("Fuel Amount: %.1f L"), fuelAmount);
            }

            // 设置显示位置
            int x = 350;
            int y = 560; 
            int width = 270;
            int height = 30;

            // 绘制背景框
            setfillcolor(RGB(30, 30, 30));
            solidrectangle(x - 1, y - 1, x + width + 1, y + height + 1);

            // 绘制背景色
            setfillcolor(RGB(40, 40, 40));
            solidrectangle(x, y, x + width, y + height);

            // 绘制边框
            setcolor(RGB(100, 100, 100));
            rectangle(x, y, x + width, y + height);

            // 绘制文本，微调位置使其居中
            outtextxy(x + 10, y + (height - textheight(amountText)) / 2, amountText);
        }

        void update() {
            ULONGLONG currentTime = GetTickCount64();
            ULONGLONG deltaTime = currentTime - lastFrameTime;

            if (deltaTime < FRAME_TIME) {
                return;
            }

            SetWorkingImage(backBuffer);
            cleardevice();

            drawDials();
            drawButton(startButton);
            drawButton(stopButton);
            drawButton(increaseThrust);
            drawButton(decreaseThrust);
            drawStatus(engine->getIsStarting(), engine->getIsRunning());
            drawAllWarningBoxes();
            drawFuelFlow();  // 确保燃油流量显示在所有元素之后
            drawFuelAmount();  // 添加油量显示

            // 绘制警告测试按钮
            for (int i = 0; i < 14; i++) {
                drawButton(warningButtons[i]);
            }

            SetWorkingImage(NULL);
            putimage(0, 0, backBuffer);

            lastFrameTime = currentTime;
        }

        bool checkButtonClick(const Button& btn, int mouseX, int mouseY) {
            return mouseX >= btn.x && mouseX <= btn.x + btn.width &&
                mouseY >= btn.y && mouseY <= btn.y + btn.height;
        }

        // 在现有的鼠标检查代码中添加新按钮的检查
        void handleMouseClick(int mouseX, int mouseY) {
            if (checkButtonClick(startButton, mouseX, mouseY)) {
                engine->start();
            }
            else if (checkButtonClick(stopButton, mouseX, mouseY)) {
                engine->stop();
            }
            else if (checkButtonClick(increaseThrust, mouseX, mouseY)) {
                engine->increaseThrust();
            }
            else if (checkButtonClick(decreaseThrust, mouseX, mouseY)) {
                engine->decreaseThrust();
            }

            // 检查警告测试按钮
            for (int i = 0; i < 14; i++) {
                if (checkButtonClick(warningButtons[i], mouseX, mouseY)) {
                    switch (i) {
                    case 0: // Single N1 Sensor
                        engine->getSpeedSensorL1()->setValidity(false);  // 只让L1失效
                        engine->getSpeedSensorL2()->setValidity(true);   
                        engine->getSpeedSensorR1()->setValidity(true);   
                        engine->getSpeedSensorR2()->setValidity(true);   
                        engine->checkWarnings(engine->getCurrentTime());
                        break;
                    case 1: // Dual N1 Sensors
                        engine->getSpeedSensorL1()->setValidity(false);
                        engine->getSpeedSensorL2()->setValidity(false);
                        engine->checkWarnings(engine->getCurrentTime());
                        break;
                    case 2: // Single EGT Sensor
                        engine->getEgtSensorL1()->setValidity(false);  // 只让L1失效
                        engine->getEgtSensorL2()->setValidity(true);   
                        engine->getEgtSensorR1()->setValidity(true);   
                        engine->getEgtSensorR2()->setValidity(true);   
                        engine->checkWarnings(engine->getCurrentTime());
                        break;
                    case 3: // Dual EGT Sensors
                        engine->getEgtSensorL1()->setValidity(false);  // L1失效
                        engine->getEgtSensorL2()->setValidity(false);  // L2失效
                        engine->getEgtSensorR1()->setValidity(true);   // R1保持正常
                        engine->getEgtSensorR2()->setValidity(true);   // R2保持正常
                        engine->checkWarnings(engine->getCurrentTime());
                        break;
                    case 4: // Low Fuel
                        engine->setFuelAmount(900);  // 设置接近低油量警告阈值
                        break;
                    case 5: // Fuel Sensor
                        engine->getFuelSensor()->setValidity(false);
                        break;
                    case 6: // High Fuel Flow
                        if (engine->getIsRunning()) {
                            engine->setFuelFlow(52.0);  // 设置超过燃油流量限制
                            engine->checkWarnings(engine->getCurrentTime());
                            engine->increaseThrust();
                            engine->decreaseThrust();
                            //此处认为故障时可能是漏油不改变温度和转速
                        }
                        break;
                    case 7: // N1 Overspeed L1
                        if (engine->getIsRunning()) {
                            engine->increaseThrust();
                            engine->decreaseThrust();
                            engine->setN1(107.0);  // 设置N1超过105%但小于120%
                        }
                        break;
                    case 8: // N1 Overspeed L2
                        if (engine->getIsRunning()) {
                            engine->increaseThrust();
                            engine->decreaseThrust();
                            engine->setN1(130.0);  // 设置N1超过120%
                        }
                        break;
                    case 9: // EGT Start L1
                        if (engine->getIsStarting()) {
                            engine->setTemperature(880.0);  // 设置启动时EGT接近850度警告值
                        }
                        break;
                    case 10: // EGT Start L2
                        if (engine->getIsStarting()) {
                            engine->setTemperature(1020.0);  // 设置启动时EGT超过1000度
                        }
                        break;
                    case 11: // EGT Run L1
                        if (engine->getIsRunning()) {
                            engine->increaseThrust();
                            engine->decreaseThrust();
                            engine->setTemperature(970.0);  // 设置运行时EGT接近950度警告值
                        }
                        break;
                    case 12: // EGT Run L2
                        if (engine->getIsRunning()) {
                            engine->increaseThrust();
                            engine->decreaseThrust();
                            engine->setTemperature(1120.0);  // 设置运行时EGT超过1100度
                        }
                        break;
                    case 13: // Reset All
                        // 重置所有参数到正常值
                        engine->resetSensors();
                        if (engine->getIsRunning()) {
                            engine->setN1(95.0);
                            engine->setTemperature(850.0);
                            engine->setFuelFlow(40.0);
                        }
                        engine->setFuelAmount(20000.0);
                        break;
                    }
                }
            }
        }
    };


    int main() {
        try {
            srand(static_cast<unsigned int>(time(nullptr)));

            Engine engine;
            GUI gui(800, 600, &engine);
            DataLogger logger;

            ULONGLONG lastUpdateTime = GetTickCount64();
            ULONGLONG currentTime;
            const double fixedTimeStep = 1.0 / 200.0;  // 5ms
            double accumulator = 0.0;
            double simulationTime = 0.0;  // 模拟时间

            bool running = true;

            // 预热循环
            for (int i = 0; i < 100; i++) {
                engine.update(fixedTimeStep);
            }

            while (running) {
                currentTime = GetTickCount64();
                double deltaTime = (currentTime - lastUpdateTime) / 1000.0;
                if (deltaTime > 0.25) deltaTime = 0.25;
                lastUpdateTime = currentTime;

                accumulator += deltaTime;

                // 处理鼠标事件
                while (MouseHit()) {
                    MOUSEMSG msg = GetMouseMsg();
                    if (msg.uMsg == WM_LBUTTONDOWN) {
                        gui.handleMouseClick(msg.x, msg.y);
                    }
                }

                // 更新引擎状态
                while (accumulator >= fixedTimeStep) {
                    engine.update(fixedTimeStep);
                    engine.checkWarnings(currentTime / 1000.0);

                    // 记录日志 - 确保模拟时间一致
                    logger.logData(
                        simulationTime,
                        engine.getN1(),
                        engine.getTemperature(),
                        engine.getFuelFlow(),
                        engine.getFuelAmount()
                    );

                    accumulator -= fixedTimeStep;
                    simulationTime += fixedTimeStep;  // 只在这里增加模拟时间
                }

                gui.update();

                if (GetAsyncKeyState(VK_ESCAPE)) {
                    running = false;
                }

                // 确保循环时间
                ULONGLONG endTime = GetTickCount64();
                ULONGLONG elapsedTime = endTime - currentTime;
                if (elapsedTime < 5) {
                    Sleep(5 - elapsedTime);
                }
            }

            closegraph();
            return 0;
        }
        catch (const std::exception& e) {
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, NULL, 0);
            wchar_t* wstrError = new wchar_t[size_needed];
            MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, wstrError, size_needed);

            MessageBox(NULL, wstrError, L"Error", MB_OK | MB_ICONERROR);

            delete[] wstrError;
            return 1;
        }
    }