// EICAS.cpp
#include <iostream>
#include <windows.h>
#include <graphics.h>
#include <conio.h>
#include <math.h>
#include <fstream>
#include <ctime>
#include <string>
#include <vector>
#include <iomanip>  // 为setprecision锟斤拷fixed锟斤拷锟斤拷头锟侥硷拷
#include <map>
#include "DataLogger.h"  // 娣诲姞澶存枃浠跺寘鍚�
#include "WarningTypes.h"  // 娣诲姞澶存枃浠跺寘鍚�
using namespace std;

#define PI 3.14159265358979323846

// 募头痈婢�系统囟

// 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
class Sensor {
protected:
    double value;
    bool isValid;
public:
    Sensor() : value(0), isValid(true) {}
    virtual double getValue() { 
        // 当传感器失效时返回0
        return isValid ? value : 0.0; 
    }
    virtual void setValue(double v) { value = v; }
    bool getValidity() { return isValid; }
    void setValidity(bool valid) { 
        isValid = valid;
        // 如果传感器变为无效，立即将值设为0
        if (!valid) {
            value = 0.0;
        }
    }
};

// 然 Engine 确使 DataLogger
class Engine {
private:
    Sensor* speedSensor1;
    Sensor* speedSensor2;
    Sensor* egtSensor1;
    Sensor* egtSensor2;
    Sensor* fuelSensor;  // 燃痛
    double fuelFlow;
    double fuelAmount;
    bool isRunning;
    bool isStarting;
    bool isStopping;  // 停状态
    bool isThrusting;  // 浠�状态
    double stopStartTime;  // 始停时
    static const double RATED_SPEED; // ??转
    static const double T0;  // 始
    static const double STOP_DURATION;  // 停时洌�10

    // 械某
    double N1;        // 转
    double temperature; // 露
    double prevFuelAmount; // 一时燃
    double timeStep;      // 时洳�
    vector<WarningMessage> warnings;
    double lastWarningTime;
    double accumulatedTime;  // 刍时
    const double THRUST_FUEL_STEP = 1.0;  // 每浠�燃
    const double THRUST_PARAM_MIN_CHANGE = 0.03;  // 小浠� 3%
    const double THRUST_PARAM_MAX_CHANGE = 0.05;  // 浠� 5%
    DataLogger* logger;  // 志录指
    map<string, double> lastLogTimes;  // 每志录时
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
    Sensor* getSpeedSensor2() { return speedSensor2; }
    Sensor* getEgtSensor1() { return egtSensor1; }
    Sensor* getEgtSensor2() { return egtSensor2; }
    double getN1() const { return N1; }
    double getTemperature() const { return temperature; }
    double getFuelFlow() const { return fuelFlow; }
    double getFuelAmount() const { return fuelAmount; }
    bool getIsRunning() const { return isRunning; }
    bool getIsStarting() const { return isStarting; }

    void stop() {
        isRunning = false;
        isStarting = false;
        isStopping = true;  // 停
        stopStartTime = accumulatedTime;  // 录停始时
        fuelFlow = 0;  // 燃直
    }

    void updateFuelFlow() {
        if (fuelFlow > 0) {
            fuelAmount -= fuelFlow * timeStep;
            if (fuelAmount < 0) fuelAmount = 0;

            // 燃
            fuelSensor->setValue(fuelAmount);
        }
    }
    Engine() {
        speedSensor1 = new Sensor();
        speedSensor2 = new Sensor();
        egtSensor1 = new Sensor();
        egtSensor2 = new Sensor();
        fuelSensor = new Sensor();  // 始燃
        fuelFlow = 0;
        fuelAmount = 20000;
        isRunning = false;
        isStarting = false;
        lastWarningTime = 0;

        // 始
        N1 = 0;
        temperature = T0;  // T0
        prevFuelAmount = fuelAmount;
        timeStep = 0.005;  // 5ms
        accumulatedTime = 0.0;  // 始刍时
        isStopping = false;
        stopStartTime = 0;
        isThrusting = false;  // 始

        // 始EGT
        egtSensor1->setValue(T0);
        egtSensor2->setValue(T0);
        logger = new DataLogger();  // 志录
        lastWarningTimes.clear();  // 确保警告时间映射为空
    }

    Sensor* getSpeedSensor1() { return speedSensor1; }

    void updateStartPhase(double timeStep) {
        isThrusting = false;
        // 刍时
        accumulatedTime += timeStep;

        // 
        std::cout << "Time step: " << timeStep << ", Accumulated time: " << accumulatedTime << std::endl;

        if (accumulatedTime <= 2.0) {
            // 1
            double speed = 10000.0 * accumulatedTime;
            N1 = (speed / RATED_SPEED) * 100.0;

            speedSensor1->setValue(speed);
            speedSensor2->setValue(speed);
            fuelFlow = 5.0 * accumulatedTime;
            temperature = T0;  // 2露

            egtSensor1->setValue(temperature);
            egtSensor2->setValue(temperature);

            isStarting = true;
        }
        else {
            // 2
            double t = accumulatedTime - 2.0;  // t0始时
            double speed = 20000.0 + 23000.0 * log10(1.0 + t);
            N1 = (speed / RATED_SPEED) * 100.0;

            speedSensor1->setValue(speed);
            speedSensor2->setValue(speed);
            fuelFlow = 10.0 + 42.0 * log10(1.0 + t);

            // 愎� T = 900*lg(t-1) + T0
            temperature = 900.0 * log10(t + 1.0) + T0;

            double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;
            double displaySpeed = speed * randFactor;

            speedSensor1->setValue(displaySpeed);
            speedSensor2->setValue(displaySpeed);
            temperature *= randFactor;

            egtSensor1->setValue(temperature);
            egtSensor2->setValue(temperature);

            // N195%转
            if (N1 >= 95.0) {
                isStarting = false;
                isRunning = true;
                std::cout << "Reached running state" << std::endl;
            }
        }

        updateFuelFlow();
    }

    void start() {
        isRunning = false;  // 锟絝alse
        isStarting = true;
        isStopping = false;  // 停
        accumulatedTime = 0.0;  // 刍时
    }

    void update(double timeStep) {
        accumulatedTime += timeStep;

        if (isStarting) {
            updateStartPhase(timeStep);
        }
        else if (isRunning) {
            // 燃
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
        // 锟斤拷锟叫阶段Ｚ�%
        double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;  // 0.971.03

        // 转
        double baseSpeed = RATED_SPEED * 0.95;  // 95%
        double speed = baseSpeed * randFactor;
        N1 = (speed / RATED_SPEED) * 100.0;

        // 
        speedSensor1->setValue(speed);
        speedSensor2->setValue(speed);

        // 露 - 
        double targetTemp = 850.0;  // 态
        double tempDiff = targetTemp - temperature;
        double tempAdjustRate = 0.1;  // 

        // 
        temperature += tempDiff * tempAdjustRate;
        // 
        temperature *= randFactor;

        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // 燃
        if (fuelFlow > 0) {
            fuelFlow = 40.0 * randFactor;  // 40
            updateFuelFlow();
        }
    }

    void updateNewRunningPhase(double timeStep) {

        // 1%~3%
        double randFactor = 1.0 + (rand() % 4 - 2) / 100.0;

        // 
        double speed = (N1 / 100.0) * RATED_SPEED * randFactor;
        speedSensor1->setValue(speed);
        speedSensor2->setValue(speed);

        egtSensor1->setValue(temperature * randFactor);
        egtSensor2->setValue(temperature * randFactor);

        // 燃
        if (fuelFlow > 0) {
            updateFuelFlow();
        }

        // 
        std::cout << "New Running Phase - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow
            << ", Time Step: " << timeStep << std::endl;
    }

    void updateStopPhase(double timeStep) {
        isThrusting = false;
        double elapsedStopTime = accumulatedTime - stopStartTime;

        if (elapsedStopTime >= STOP_DURATION) {
            // 停
            isStopping = false;
            N1 = 0;
            temperature = T0;  // 
            return;
        }

        // 
        const double a = 0.9;  // 

        // 
        double decayRatio = pow(a, elapsedStopTime);

        // 转
        N1 = N1 * decayRatio;
        temperature = T0 + (temperature - T0) * decayRatio;

        // 
        double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;
        double displaySpeed = (N1 / 100.0) * RATED_SPEED * randFactor;

        speedSensor1->setValue(displaySpeed);
        speedSensor2->setValue(displaySpeed);
        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // 
        std::cout << "Stop Phase - Time: " << elapsedStopTime
            << "s, N1: " << N1
            << "%, Temp: " << temperature << std::endl;
    }

    void checkWarnings(double currentTime) {
        // 锟斤拷效
        if (!speedSensor1->getValidity() && speedSensor2->getValidity()) {
            addWarning("Single N1 Sensor Failure", NORMAL, currentTime);
        }

        if (speedSensor1->getValidity() && !speedSensor2->getValidity()) {
            addWarning("Engine N1 Sensor Failure", CAUTION, currentTime);
        }

        if (!egtSensor1->getValidity() && egtSensor2->getValidity()) {
            addWarning("Single EGT Sensor Failure", NORMAL, currentTime);
        }

        if (egtSensor1->getValidity() && !egtSensor2->getValidity()) {
            addWarning("Engine EGT Sensor Failure", CAUTION, currentTime);
        }

        // 双锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟较硷拷锟�
        if ((!speedSensor1->getValidity() && !speedSensor2->getValidity()) ||
            (!egtSensor1->getValidity() && !egtSensor2->getValidity())) {
            addWarning("Dual Engine Sensor Failure - Shutdown", WARNING, currentTime);
            stop();
        }

        // 燃锟斤拷系统锟斤拷锟斤拷锟斤拷锟
        if (fuelAmount < 1000 && isRunning) {
            addWarning("Low Fuel Level", CAUTION, currentTime);
        }

        if (!fuelSensor->getValidity()) {
            addWarning("Fuel Sensor Failure", WARNING, currentTime);
        }

        if (fuelFlow > 50) {
            addWarning("Fuel Flow Exceeded Limit", CAUTION, currentTime);
        }

        // 转锟劫硷拷锟�
        double n1Percent = getN1();
        if (n1Percent > 120) {
            addWarning("N1 Overspeed Level 2 - Engine Shutdown", WARNING, currentTime);
            stop();
        }
        else if (n1Percent > 105) {
            addWarning("N1 Overspeed Level 1", CAUTION, currentTime);
        }

        // 锟铰度硷拷锟�
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
            return;  // �������锟斤拷锟饺讹拷锟斤拷锟斤拷状态锟��拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
        }
        isThrusting = true;

        // 锟斤拷锟斤拷燃锟斤拷锟斤拷锟斤拷
        fuelFlow += THRUST_FUEL_STEP;

        // 锟斤拷锟斤拷3%~5%之锟斤拷锟斤拷锟斤拷锟戒化锟斤拷
        double changeRatio = THRUST_PARAM_MIN_CHANGE +
            (static_cast<double>(rand()) / RAND_MAX) *
            (THRUST_PARAM_MAX_CHANGE - THRUST_PARAM_MIN_CHANGE);

        // 锟斤拷锟斤拷转锟劫猴拷锟铰讹拷
        double baseSpeed = RATED_SPEED * N1 / 100.0;  // 锟斤拷准转锟斤拷
        double newSpeed = baseSpeed * (1.0 + changeRatio);
        N1 = (newSpeed / RATED_SPEED) * 100.0;


        // 锟斤拷锟斤拷N1锟斤拷锟斤拷锟斤拷锟斤拷100%
        if (N1 > 125.0) {
            N1 = 125.0;
            newSpeed = RATED_SPEED;
        }

        // 锟斤拷锟铰达拷锟斤拷锟斤拷锟斤拷示值
        speedSensor1->setValue(newSpeed);
        speedSensor2->setValue(newSpeed);

        // 锟斤拷锟斤拷锟铰讹拷
        temperature = temperature * (1.0 + changeRatio);

        // 锟斤拷锟斤拷EGT锟斤拷锟斤拷锟斤拷
        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // ���斤拷���斤拷锟斤拷锟�
        std::cout << "Thrust increased - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow << std::endl;
    }

    void decreaseThrust() {
        if (!isRunning || isStarting || isStopping) {
            return;  // 只锟斤拷锟饺讹拷锟斤拷锟斤拷状态锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
        }
        isThrusting = true;

        // 确锟斤拷燃锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟叫≈
        if (fuelFlow > THRUST_FUEL_STEP) {
            fuelFlow -= THRUST_FUEL_STEP;
        }

        // 锟斤拷锟斤拷3%~5%之锟斤拷锟斤拷锟斤化锟斤拷
        double changeRatio = THRUST_PARAM_MIN_CHANGE +
            (static_cast<double>(rand()) / RAND_MAX) *
            (THRUST_PARAM_MAX_CHANGE - THRUST_PARAM_MIN_CHANGE);

        // 锟斤拷锟斤拷转锟劫猴拷锟铰讹拷
        double baseSpeed = RATED_SPEED * N1 / 100.0;  // 锟斤拷准转锟斤拷
        double newSpeed = baseSpeed * (1.0 - changeRatio);
        N1 = (newSpeed / RATED_SPEED) * 100.0;

        // 锟斤拷锟斤拷N1锟斤拷锟斤拷锟斤拷锟斤拷小值锟斤拷锟斤拷锟斤拷60%锟斤拷
        if (N1 < 0) {
            N1 = 0;
            newSpeed = 0;
        }

        // 锟斤拷锟铰达拷锟斤拷锟斤拷锟斤拷示值
        speedSensor1->setValue(newSpeed);
        speedSensor2->setValue(newSpeed);

        // 锟斤拷锟斤拷锟铰度ｏ拷锟斤拷准锟铰讹拷850锟斤拷
        temperature = temperature * (1.0 - changeRatio);

        // 锟斤拷锟斤拷EGT锟斤拷锟斤拷锟斤拷
        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // 锟斤拷锟斤拷锟
        std::cout << "Thrust decreased - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow << std::endl;
    }

    // 锟斤拷锟接达拷锟斤拷锟斤拷锟斤拷锟斤拷模锟解方锟斤拷
    void simulateSensorFailure(int sensorType) {
        switch (sensorType) {
        case 0: // 锟斤拷锟斤拷转锟劫达拷锟斤拷锟斤拷锟斤拷锟斤拷
            speedSensor1->setValidity(false);
            break;
        case 1: // 锟斤拷锟斤拷转锟劫达拷锟斤拷锟斤拷锟斤拷锟斤拷
            speedSensor1->setValidity(false);
            speedSensor2->setValidity(false);
            break;
        case 2: // 锟斤拷锟斤拷EGT锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
            egtSensor1->setValidity(false);
            break;
        case 3: // 锟��拷锟斤拷EGT锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
            egtSensor1->setValidity(false);
            egtSensor2->setValidity(false);
            break;
            // ... 锟斤拷锟斤拷锟斤拷锟斤拷
        }
    }

    double getCurrentTime() const {
        return static_cast<double>(GetTickCount64()) / 1000.0;
    }

    // 锟斤拷锟斤拷燃锟酵达拷锟斤拷锟斤拷锟斤拷锟侥凤拷锟绞凤拷锟斤拷
    Sensor* getFuelSensor() { return fuelSensor; }

    // 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷删锟斤拷燃锟酵达拷锟斤拷锟斤拷
    ~Engine() {
        delete speedSensor1;
        delete speedSensor2;
        delete egtSensor1;
        delete egtSensor2;
        delete fuelSensor;  // 删锟斤拷燃锟酵达拷锟斤拷锟斤拷
        if (logger) {
            delete logger;  // 锟斤拷全删锟斤拷锟斤拷志锟斤拷录锟斤拷
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
        speedSensor1->setValidity(true);
        speedSensor2->setValidity(true);
        egtSensor1->setValidity(true);
        egtSensor2->setValidity(true);
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
const double Engine::STOP_DURATION = 10.0;  // 10锟斤拷停锟斤拷时锟斤拷

// GUI锟斤拷
class GUI {
private:
    static const int WINDOW_WIDTH = 1600;    // 锟斤拷锟接达拷锟节匡拷锟斤拷
    static const int WINDOW_HEIGHT = 900;    // 锟斤拷锟接达拷锟节高讹拷
    static const int DIAL_RADIUS = 60;       // 锟斤拷锟斤拷锟斤拷锟教达拷小
    static const int DIAL_SPACING_X = 180;   // 锟斤拷锟斤拷锟斤拷锟斤拷水平锟斤拷
    static const int DIAL_SPACING_Y = 180;   // 锟斤拷锟斤拷锟斤拷锟斤拷啵锟斤拷140锟斤拷为180
    static const int DIAL_START_X = 100;     // 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷始位锟斤拷
    static const int DIAL_START_Y = 150;
    static const int WARNING_BOX_WIDTH = 380;  // 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
    static const int WARNING_BOX_HEIGHT = 30;
    static const int WARNING_START_X = 750;
    static const int WARNING_START_Y = 100;
    static const int WARNING_SPACING = 5;
    static const int REFRESH_RATE = 200;  // 200Hz = 5ms刷锟斤拷一锟斤拷
    static const int FRAME_TIME = 1000 / REFRESH_RATE;  // 5ms
    ULONGLONG lastFrameTime;
    IMAGE* backBuffer;
    Engine* engine;  // 确锟斤拷 engine 指锟诫被锟斤拷确锟斤拷锟斤拷

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
        // 确锟斤拷 engine 指锟斤拷锟斤拷效
        if (!eng) {
            throw std::runtime_error("Invalid engine pointer");
        }

        // 锟斤拷锟饺��拷始锟斤拷钮锟斤拷锟斤拷锟斤拷预锟斤拷时锟斤拷锟斤拷使锟斤拷
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
            _T("Reset Sensors")    // 重置所有传感器
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

        // 锟斤拷始锟斤拷图锟斤拷系统
        initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
        setbkcolor(RGB(40, 40, 40));
        cleardevice();

        // 锟斤拷锟斤拷锟襟备伙拷锟斤拷锟斤拷
        backBuffer = new IMAGE(WINDOW_WIDTH, WINDOW_HEIGHT);
        if (!backBuffer) {
            throw std::runtime_error("Failed to create back buffer");
        }

        SetWorkingImage(backBuffer);
        cleardevice();
        SetWorkingImage(NULL);

        // 预锟饺伙拷图系统
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
        // 锟��拷锟斤拷锟斤拷锟斤拷锟斤拷圆锟轿碉拷锟���拷
        setcolor(RGB(60, 60, 60));
        setfillcolor(RGB(20, 20, 20));
        fillcircle(x, y, radius);

        // 锟斤拷锟斤拷转锟劫憋拷锟教猴拷EGT锟斤拷锟教碉拷锟斤拷锟解处锟斤拷
        bool isN1Dial = _tcsstr(label, _T("N1")) != nullptr;
        bool isEGTDial = _tcsstr(label, _T("EGT")) != nullptr;
        double valueToDisplay = value;
        double maxValueToDisplay = maxValue;
        bool isStarting = engine->getIsStarting();
        int arcDegrees = 360;  // 默锟斤拷为360锟斤拷

        if (isN1Dial) {
            valueToDisplay = (value / 40000.0) * 100;  // 40000锟角额定转锟斤拷
            maxValueToDisplay = 125;  // 锟斤拷锟�125%
            arcDegrees = 210;  // N1锟斤拷锟斤拷使锟斤拷210锟斤拷
        }
        else if (isEGTDial) {
            maxValueToDisplay = 1200;  // EGT锟斤拷锟街�1200锟斤拷
            arcDegrees = 210;   // EGT锟斤拷锟斤拷也使锟斤拷210锟饺伙拷锟斤拷
        }

        // 锟斤拷锟狡刻讹拷
        int startAngle = 90;  // 锟斤拷始锟角度ｏ拷12锟斤拷锟斤拷位锟矫ｏ拷

        // 锟斤拷锟斤拷锟斤拷锟教讹拷锟���拷
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

        // 锟斤拷锟狡次刻讹拷锟斤拷
        for (int i = 0; i <= arcDegrees; i += 6) {
            if (i % 30 != 0) {  // 锟斤拷锟斤拷锟斤拷锟教讹拷位
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

        // 锟斤拷锟斤拷值锟斤拷状态选锟斤拷锟斤拷色
        COLORREF valueColor = RGB(255, 255, 255);  // 默锟较帮拷色
        if (isN1Dial) {
            if (valueToDisplay > 105) valueColor = RGB(255, 128, 0);
            if (valueToDisplay > 120) valueColor = RGB(255, 0, 0);
        }
        else if (isEGTDial) {
            if (isStarting) {
                // 锟斤拷锟斤拷锟斤拷锟斤拷锟叫碉拷锟铰度撅拷锟斤拷
                if (valueToDisplay > 1000) {
                    valueColor = RGB(255, 0, 0);      // 锟斤拷色锟斤拷锟芥（锟斤拷锟斤拷2锟斤拷
                }
                else if (valueToDisplay > 850) {
                    valueColor = RGB(255, 128, 0);    // 锟斤拷锟斤拷色锟斤拷锟芥（锟斤拷锟斤拷1锟斤拷
                }
            }
            else {
                // 锟斤拷态锟斤拷锟斤拷时锟斤拷锟铰度撅拷锟斤拷
                if (valueToDisplay > 1100) {
                    valueColor = RGB(255, 0, 0);      // 锟斤拷色锟斤拷锟芥（锟斤拷锟斤拷4锟���拷
                }
                else if (valueToDisplay > 950) {
                    valueColor = RGB(255, 128, 0);    // 锟斤拷锟斤拷色锟斤拷锟芥（锟斤拷锟斤拷3锟斤拷
                }
            }
        }

        // 锟斤拷锟斤拷值锟侥伙拷锟轿憋拷锟斤拷
        double angleRatio = isN1Dial ? (valueToDisplay / maxValueToDisplay) : (valueToDisplay / maxValue);
        angleRatio = min(angleRatio, 1.0);
        double angle = angleRatio * arcDegrees;

        // 使������斤拷锟斤拷色锟斤拷锟狡伙拷锟斤拷
        setfillcolor(valueColor);
        setcolor(valueColor);
        POINT* points = new POINT[362];
        points[0].x = x;
        points[0].y = y;

        for (int i = 0; i <= angle; i++) {
            double rad = (90 - i) * PI / 180;
            points[i + 1].x = x + (radius - 15) * cos(rad);  // 锟斤拷小锟斤拷锟诫径锟斤拷锟斤拷锟斤拷锟教度空硷拷
            points[i + 1].y = y - (radius - 15) * sin(rad);
        }

        solidpolygon(points, static_cast<int>(angle) + 2);
        delete[] points;

        // 锟斤拷锟斤拷指锟斤拷
        double pointerAngle = (90 - angle) * PI / 180;
        int pointerLength = radius - 10;
        int pointerX = x + pointerLength * cos(pointerAngle);
        int pointerY = y - pointerLength * sin(pointerAngle);

        // 锟斤拷锟斤拷指锟斤拷锟斤拷影
        setcolor(RGB(20, 20, 20));
        setlinestyle(PS_SOLID, 3);
        line(x, y, pointerX + 2, pointerY + 2);

        // 锟斤拷锟斤拷指锟诫本锟斤拷
        setcolor(WHITE);
        setlinestyle(PS_SOLID, 2);
        line(x, y, pointerX, pointerY);

        // 锟斤拷锟斤拷锟斤拷锟斤拷装锟斤拷
        setfillcolor(RGB(40, 40, 40));
        fillcircle(x, y, 8);
        setfillcolor(valueColor);
        fillcircle(x, y, 4);

        // 锟斤拷锟狡憋拷签锟斤拷��斤拷值
        settextstyle(20, 0, _T("Arial"));
        settextcolor(valueColor);
        TCHAR valueText[32];
        if (isN1Dial) {
            _stprintf_s(valueText, _T("%s: %.1f%%"), label, valueToDisplay);
        }
        else {
            _stprintf_s(valueText, _T("%s: %.1f"), label, value);
        }
        outtextxy(x - 40, y + radius + 10, valueText);
    }

    void drawDials() {
        // 锟斤拷一锟斤拷 - 锟襟发讹拷锟斤拷锟斤拷锟斤拷锟斤拷
        drawArcDial(DIAL_START_X, DIAL_START_Y, DIAL_RADIUS,
            engine->getSpeedSensor1()->getValue(), 40000, _T("N1-L1"), RGB(0, 255, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X, DIAL_START_Y, DIAL_RADIUS,
            engine->getSpeedSensor2()->getValue(), 40000, _T("N1-L2"), RGB(0, 255, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X * 2, DIAL_START_Y, DIAL_RADIUS,
            engine->getEgtSensor1()->getValue(), 1200, _T("EGT-L1"), RGB(255, 128, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X * 3, DIAL_START_Y, DIAL_RADIUS,
            engine->getEgtSensor2()->getValue(), 1200, _T("EGT-L2"), RGB(255, 128, 0));

        // 锟节讹拷锟斤拷 - 锟揭凤拷锟斤拷锟斤拷锟斤拷锟斤拷?? (锟斤拷锟斤拷锟剿达拷直锟斤拷锟�)
        drawArcDial(DIAL_START_X, DIAL_START_Y + DIAL_SPACING_Y, DIAL_RADIUS,
            engine->getSpeedSensor1()->getValue(), 40000, _T("N1-R1"), RGB(0, 255, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X, DIAL_START_Y + DIAL_SPACING_Y, DIAL_RADIUS,
            engine->getSpeedSensor2()->getValue(), 40000, _T("N1-R2"), RGB(0, 255, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X * 2, DIAL_START_Y + DIAL_SPACING_Y, DIAL_RADIUS,
            engine->getEgtSensor1()->getValue(), 1200, _T("EGT-R1"), RGB(255, 128, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X * 3, DIAL_START_Y + DIAL_SPACING_Y, DIAL_RADIUS,
            engine->getEgtSensor2()->getValue(), 1200, _T("EGT-R2"), RGB(255, 128, 0));
    }

    void drawButton(const Button& btn) {
        // 锟斤���锟狡帮拷钮锟斤拷锟斤拷
        setfillcolor(btn.pressed ? RGB(150, 150, 150) : btn.bgColor);
        solidroundrect(btn.x, btn.y, btn.x + btn.width, btn.y + btn.height, 10, 10);

        // 锟斤拷锟狡帮拷钮锟斤拷
        settextstyle(20, 0, _T("Arial"));
        setbkmode(TRANSPARENT);
        settextcolor(btn.textColor);

        // 锟斤拷锟斤拷锟斤拷锟斤拷位锟斤拷使锟斤拷锟斤拷锟�
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
        case CAUTION: boxColor = RGB(255, 128, 0); break;  // ���斤拷锟斤拷色
        case WARNING: boxColor = RGB(255, 0, 0); break;    // 锟斤拷色
        default: boxColor = RGB(128, 128, 128);            // 锟斤拷色
        }

        // 锟斤拷锟狡撅拷锟斤拷锟竭匡拷效锟斤拷
        // 锟斤拷呖锟�
        setfillcolor(RGB(30, 30, 30));
        solidrectangle(x - 2, y - 2, x + WARNING_BOX_WIDTH + 2, y + WARNING_BOX_HEIGHT + 2);

        // 锟节诧拷锟斤拷???
        setfillcolor(isActive ? RGB(60, 60, 60) : RGB(40, 40, 40));
        solidrectangle(x, y, x + WARNING_BOX_WIDTH, y + WARNING_BOX_HEIGHT);

        // 锟斤拷锟狡边匡拷
        setcolor(isActive ? boxColor : RGB(80, 80, 80));
        rectangle(x - 1, y - 1, x + WARNING_BOX_WIDTH + 1, y + WARNING_BOX_HEIGHT + 1);

        // 锟斤拷锟狡撅拷锟斤拷锟侥憋拷
        settextstyle(16, 0, _T("Arial"));
        settextcolor(isActive ? boxColor : RGB(128, 128, 128));
        wstring wstr(message.begin(), message.end());
        outtextxy(x + 10, y + 5, wstr.c_str());

        // 锟斤拷锟斤拷锟斤拷婕わ拷睿�锟斤拷锟斤拷锟斤拷锟斤拷锟街甘撅拷锟
        if (isActive) {
            setfillcolor(boxColor);
            solidrectangle(x, y, x + 3, y + WARNING_BOX_HEIGHT);
        }
    }

    void drawAllWarningBoxes() {
        int x = WARNING_START_X;
        int y = WARNING_START_Y;
        const vector<WarningMessage>& warnings = engine->getWarnings();

        // 锟斤拷锟斤拷锟斤拷锟叫匡拷锟杰的撅拷锟斤拷
        struct WarningDef {
            string message;
            WarningLevel level;
        };

        WarningDef allWarnings[] = {
            // 锟斤拷锟斤拷锟斤拷锟届常
            {"Single N1 Sensor Failure", NORMAL},           // 锟斤拷锟斤拷转锟劫达拷锟斤拷锟斤拷锟斤拷锟斤拷
            {"Engine N1 Sensor Failure", CAUTION},          // 锟斤拷锟斤拷转锟劫达拷锟斤拷锟斤拷锟斤拷锟斤拷
            {"Single EGT Sensor Failure", NORMAL},          // 锟斤拷锟斤拷EGT锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
            {"Engine EGT Sensor Failure", CAUTION},         // 锟斤拷锟斤拷EGT锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
            {"Dual Engine Sensor Failure - Shutdown", WARNING},  // 双锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷

            // 燃锟斤拷锟届常
            {"Low Fuel Level", CAUTION},                    // 燃锟斤拷锟斤拷锟斤拷锟斤拷
            {"Fuel Sensor Failure", WARNING},               // 燃锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
            {"Fuel Flow Exceeded Limit", CAUTION},          // 燃锟斤拷锟斤拷锟劫筹拷锟斤拷

            // 转锟斤拷锟届常
            {"N1 Overspeed Level 1", CAUTION},             // 锟斤拷转1
            {"N1 Overspeed Level 2 - Engine Shutdown", WARNING}, // 锟斤拷转2

            // 锟铰讹拷锟届常
            {"EGT Overtemp Level 1 During Start", CAUTION},      // 锟斤拷锟斤拷1
            {"EGT Overtemp Level 2 During Start - Shutdown", WARNING}, // 锟斤拷锟斤拷2
            {"EGT Overtemp Level 1 During Run", CAUTION},        // 锟斤拷锟斤拷3
            {"EGT Overtemp Level 2 During Run - Shutdown", WARNING}    // 锟斤拷锟斤拷4
        };

        // 锟斤拷锟斤拷锟斤拷锟叫撅拷锟斤拷锟�
        for (const auto& warningDef : allWarnings) {
            bool isActive = false;

            // 锟斤拷��鼻帮拷锟斤拷锟斤拷欠窦せ锟�
            for (const auto& warning : warnings) {
                // 锟斤拷锟斤拷锟较⑵ワ拷洳�锟斤拷时锟斤拷锟斤拷5锟斤拷锟斤拷
                if (warning.message == warningDef.message &&
                    warning.timestamp >= engine->getCurrentTime() - 5.0) {
                    isActive = true;
                    break;
                }
            }

            drawWarningBox(warningDef.message, warningDef.level, isActive, x, y);
            y += WARNING_BOX_HEIGHT + WARNING_SPACING;

            // 每6锟斤拷锟斤拷锟芥换锟斤拷
            if (y > WARNING_START_Y + 6 * (WARNING_BOX_HEIGHT + WARNING_SPACING)) {
                x += WARNING_BOX_WIDTH + WARNING_SPACING;
                y = WARNING_START_Y;
            }
        }
    }

    void drawFuelFlow() {
        double fuelFlow = engine->getFuelFlow();

        // 锟斤拷锟斤拷燃锟斤拷锟斤拷锟劫撅拷锟斤拷锟斤拷色
        COLORREF textColor, borderColor, bgColor;
        if (fuelFlow > 0) {
            if (fuelFlow > 50) {
                // 燃锟斤拷锟斤拷锟劫筹拷锟斤拷 - 锟斤拷锟斤拷色锟斤拷锟斤拷
                textColor = RGB(255, 128, 0);   // 锟斤拷锟斤拷色锟斤拷锟斤拷
                borderColor = RGB(255, 128, 0); // 锟斤拷锟���拷色锟竭匡拷
                bgColor = RGB(40, 40, 40);      // 锟斤拷锟斤拷色锟斤拷锟斤拷
            }
            else {
                // 锟斤拷锟斤拷锟斤拷锟斤拷状态 - 锟斤拷色
                textColor = WHITE;              // 锟斤拷色锟斤拷锟斤拷
                borderColor = RGB(100, 100, 100); // 锟斤拷锟缴�锟竭匡拷
                bgColor = RGB(40, 40, 40);       // 锟斤拷锟斤拷色锟斤拷锟斤拷
            }
        }
        else {
            // 锟斤拷锟斤拷锟斤拷状态 - 锟斤拷色
            textColor = RGB(150, 150, 150);   // 锟斤拷色锟斤拷锟斤拷
            borderColor = RGB(100, 100, 100); // 锟斤拷锟缴�锟竭匡拷
            bgColor = RGB(40, 40, 40);       // 锟斤拷锟斤拷色锟斤拷锟斤拷
        }

        // 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷式
        settextstyle(24, 0, _T("Arial"));
        settextcolor(textColor);

        // 锟斤拷锟斤拷锟斤拷示锟侥憋拷
        TCHAR flowText[64];
        _stprintf_s(flowText, _T("Fuel Flow: %.1f kg/h"), fuelFlow);

        // 锟斤拷锟斤拷锟斤拷示位锟斤拷
        int x = 50;
        int y = 560;
        int width = 200;
        int height = 30;

        // 锟斤拷锟斤拷锟斤拷呖锟斤拷���接靶э拷锟�
        setfillcolor(RGB(30, 30, 30));
        solidrectangle(x - 1, y - 1, x + width + 1, y + height + 1);

        // 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
        setfillcolor(bgColor);
        solidrectangle(x, y, x + width, y + height);

        // 锟斤拷锟狡边匡拷
        setcolor(borderColor);
        rectangle(x, y, x + width, y + height);

        // 锟斤拷锟斤拷锟斤拷诨锟皆咀刺�锟斤拷��斤拷锟斤拷锟斤拷锟街甘撅拷锟�
        if (fuelFlow > 0) {
            setfillcolor(borderColor);
            solidrectangle(x, y, x + 3, y + height);
        }

        // 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷微锟斤拷锟斤拷偏锟斤拷锟皆避匡拷锟斤拷锟街甘撅拷锟斤拷锟
        outtextxy(x + 10, y + (height - textheight(flowText)) / 2, flowText);
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
        drawFuelFlow();  // 确锟斤拷锟斤拷锟斤拷锟斤拷元锟斤拷之锟斤拷锟斤拷锟�

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
                    engine->getSpeedSensor1()->setValidity(false);  // L1传感器失效
                    engine->getSpeedSensor2()->setValidity(true);   // 确保L2传感器正常
                    // 立即触发警告检查
                    engine->checkWarnings(engine->getCurrentTime());
                    break;
                case 1: // Dual N1 Sensors
                    engine->getSpeedSensor1()->setValidity(false);
                    engine->getSpeedSensor2()->setValidity(false);
                    engine->checkWarnings(engine->getCurrentTime());
                    break;
                case 2: // Single EGT Sensor
                    engine->getEgtSensor1()->setValidity(false);
                    engine->getEgtSensor2()->setValidity(true);
                    break;
                case 3: // Dual EGT Sensors
                    engine->getEgtSensor1()->setValidity(false);
                    engine->getEgtSensor2()->setValidity(false);
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
                    }
                    break;
                case 7: // N1 Overspeed L1
                    if (engine->getIsRunning()) {
                        engine->setN1(107.0);  // 设置N1超过105%但小于120%
                    }
                    break;
                case 8: // N1 Overspeed L2
                    if (engine->getIsRunning()) {
                        engine->setN1(122.0);  // 设置N1超过120%
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
                        engine->setTemperature(970.0);  // 设置运行时EGT接近950度警告值
                    }
                    break;
                case 12: // EGT Run L2
                    if (engine->getIsRunning()) {
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
        double simulationTime = 0.0;  // 模锟斤拷时锟斤拷

        bool running = true;

        // 预锟斤拷循锟斤拷
        for (int i = 0; i < 100; i++) {
            engine.update(fixedTimeStep);
        }

        while (running) {
            currentTime = GetTickCount64();
            double deltaTime = (currentTime - lastUpdateTime) / 1000.0;
            if (deltaTime > 0.25) deltaTime = 0.25;
            lastUpdateTime = currentTime;

            accumulator += deltaTime;

            // 锟斤拷锟斤拷锟斤拷锟斤拷
            while (MouseHit()) {
                MOUSEMSG msg = GetMouseMsg();
                if (msg.uMsg == WM_LBUTTONDOWN) {
                    gui.handleMouseClick(msg.x, msg.y);
                }
            }

            // 锟斤拷锟斤拷锟斤拷锟斤拷
            while (accumulator >= fixedTimeStep) {
                engine.update(fixedTimeStep);
                engine.checkWarnings(currentTime / 1000.0);

                // 锟斤拷录锟斤拷锟斤拷 - 使锟矫撅拷确锟斤拷模锟斤拷时锟斤拷
                logger.logData(
                    simulationTime,
                    engine.getN1(),
                    engine.getTemperature(),
                    engine.getFuelFlow(),
                    engine.getFuelAmount()
                );

                accumulator -= fixedTimeStep;
                simulationTime += fixedTimeStep;  // 只锟斤拷锟斤拷锟斤拷锟斤拷锟侥ｏ拷锟绞憋拷锟�
            }

            gui.update();

            if (GetAsyncKeyState(VK_ESCAPE)) {
                running = false;
            }

            // 锟斤拷确锟斤拷锟斤拷循锟斤拷时锟斤拷
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