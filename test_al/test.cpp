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
using namespace std;

#define PI 3.14159265358979323846

// 在文件开头添加告警系统相关定义
enum WarningLevel {
    NORMAL,     // 白色
    CAUTION,    // 琥珀色
    WARNING,    // 红色
    INVALID     // 无效
};

struct WarningMessage {
    string message;
    WarningLevel level;
    double timestamp;

    WarningMessage(const string& msg, WarningLevel lvl, double time)
        : message(msg), level(lvl), timestamp(time) {}
};

// 传感器基类
class Sensor {
protected:
    double value;
    bool isValid;
public:
    Sensor() : value(0), isValid(true) {}
    virtual double getValue() { return value; }
    virtual void setValue(double v) { value = v; }
    bool getValidity() { return isValid; }
    void setValidity(bool valid) { isValid = valid; }
};

// 发动机类
class Engine {
private:
    Sensor* speedSensor1;
    Sensor* speedSensor2;
    Sensor* egtSensor1;
    Sensor* egtSensor2;
    double fuelFlow;
    double fuelAmount;
    bool isRunning;
    bool isStarting;
    bool isStopping;  // 添加停车状态标志
    bool isThrusting;  // 添加推力变化状态标志
    double stopStartTime;  // 记录开始停车的时间
    static const double RATED_SPEED; // 额定转速
    static const double T0;  // 初始温度
    static const double STOP_DURATION;  // 停车持续时间（10秒）

    // 已有的成员变量...
    double N1;        // 转速百分比
    double temperature; // 排气温度
    double prevFuelAmount; // 上一时刻燃油量
    double timeStep;      // 时间步长
    vector<WarningMessage> warnings;
    double lastWarningTime;
    double accumulatedTime;  // 添加累积时间变量
    const double THRUST_FUEL_STEP = 1.0;  // 每次推力变化的燃油流量步进值
    const double THRUST_PARAM_MIN_CHANGE = 0.03;  // 最小变化率 3%
    const double THRUST_PARAM_MAX_CHANGE = 0.05;  // 最大变化率 5%

    void addWarning(const string& message, WarningLevel level, double currentTime) {
        if (currentTime - lastWarningTime >= 5.0 || warnings.empty() ||
            warnings.back().message != message) {
            warnings.push_back(WarningMessage(message, level, currentTime));
            lastWarningTime = currentTime;
        }
    }

public:
    // 在Engine类中???
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
        isStopping = true;  // 进入停车阶段
        stopStartTime = accumulatedTime;  // 记录停车开始时间
        fuelFlow = 0;  // 燃油流速直接归零
    }

    void updateFuelFlow() {
        if (fuelFlow > 0) {
            fuelAmount -= fuelFlow * timeStep;
            if (fuelAmount < 0) fuelAmount = 0;
        }
    }
    Engine() {
        speedSensor1 = new Sensor();
        speedSensor2 = new Sensor();
        egtSensor1 = new Sensor();
        egtSensor2 = new Sensor();
        fuelFlow = 0;
        fuelAmount = 20000;
        isRunning = false;
        isStarting = false;
        lastWarningTime = 0;

        // 初始化之前未初始化的成员变量
        N1 = 0;
        temperature = T0;  // T0是之前定义的初始温度
        prevFuelAmount = fuelAmount;
        timeStep = 0.005;  // 5ms
        accumulatedTime = 0.0;  // 初始化累积时间
        isStopping = false;
        stopStartTime = 0;
        isThrusting = false;  // 初始化推力状态

        // 初始化EGT传感器的初始值为环境温度
        egtSensor1->setValue(T0);
        egtSensor2->setValue(T0);
    }

    Sensor* getSpeedSensor1() { return speedSensor1; }

    void updateStartPhase(double timeStep) {
        isThrusting = false;
        // 累加时间
        accumulatedTime += timeStep;

        // 添加调试输出
        std::cout << "Time step: " << timeStep << ", Accumulated time: " << accumulatedTime << std::endl;

        if (accumulatedTime <= 2.0) {
            // 启动阶段1：线性增长
            double speed = 10000.0 * accumulatedTime;
            N1 = (speed / RATED_SPEED) * 100.0;

            speedSensor1->setValue(speed);
            speedSensor2->setValue(speed);
            fuelFlow = 5.0 * accumulatedTime;
            temperature = T0;  // 2秒时温度应该是 T0 

            egtSensor1->setValue(temperature);
            egtSensor2->setValue(temperature);

            isStarting = true;
        }
        else {
            // 启动阶段2：对数增长
            double t = accumulatedTime - 2.0;  // t从0开始计时
            double speed = 20000.0 + 23000.0 * log10(1.0 + t);
            N1 = (speed / RATED_SPEED) * 100.0;

            speedSensor1->setValue(speed);
            speedSensor2->setValue(speed);
            fuelFlow = 10.0 + 42.0 * log10(1.0 + t);

            // 温度计算公式为 T = 900*lg(t-1) + T0
            temperature = 900.0 * log10(t + 1.0) + T0;

            double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;
            double displaySpeed = speed * randFactor;

            speedSensor1->setValue(displaySpeed);
            speedSensor2->setValue(displaySpeed);
            temperature *= randFactor;

            egtSensor1->setValue(temperature);
            egtSensor2->setValue(temperature);

            // 只有当N1达到95%时才转换到运行状态
            if (N1 >= 95.0) {
                isStarting = false;
                isRunning = true;
                std::cout << "Reached running state" << std::endl;
            }
        }

        updateFuelFlow();
    }

    void start() {
        isRunning = false;  // 修改：先设置为false，让启动阶段正确执行
        isStarting = true;
        isStopping = false;  // 确保停车状态被关闭
        accumulatedTime = 0.0;  // 重置累积时间，确保启动阶段从头开始
    }

    void update(double timeStep) {
        accumulatedTime += timeStep;

        if (isStarting) {
            updateStartPhase(timeStep);
        }
        else if (isRunning) {
            // 根据燃油流量断是否处于新的运行阶段
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
        // 在稳定运行阶段，参数在±3%范围内波动
        double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;  // 生成0.97到1.03之间的随机因子

        // 更新转速
        double baseSpeed = RATED_SPEED * 0.95;  // 95%额定转速作为基准
        double speed = baseSpeed * randFactor;
        N1 = (speed / RATED_SPEED) * 100.0;

        // 更新传感器显示值
        speedSensor1->setValue(speed);
        speedSensor2->setValue(speed);

        // 更新温度 - 使用当前温度作为基准，逐渐趋近目标温度
        double targetTemp = 850.0;  // 目标稳态温度
        double tempDiff = targetTemp - temperature;
        double tempAdjustRate = 0.1;  // 温度调整速率

        // 平滑过渡到目标温度
        temperature += tempDiff * tempAdjustRate;
        // 添加小幅波动
        temperature *= randFactor;

        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // 更新燃油量
        if (fuelFlow > 0) {
            fuelFlow = 40.0 * randFactor;  // 基准燃油流量40
            updateFuelFlow();
        }
    }

    void updateNewRunningPhase(double timeStep) {

        // 添加小幅随机波动（1%~3%）
        double randFactor = 1.0 + (rand() % 4 - 2) / 100.0;

        // 更新传感器显示值
        double speed = (N1 / 100.0) * RATED_SPEED * randFactor;
        speedSensor1->setValue(speed);
        speedSensor2->setValue(speed);

        egtSensor1->setValue(temperature * randFactor);
        egtSensor2->setValue(temperature * randFactor);

        // 更新燃油流量
        if (fuelFlow > 0) {
            updateFuelFlow();
        }

        // 调试输出
        std::cout << "New Running Phase - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow
            << ", Time Step: " << timeStep << std::endl;
    }

    void updateStopPhase(double timeStep) {
        isThrusting = false;
        double elapsedStopTime = accumulatedTime - stopStartTime;

        if (elapsedStopTime >= STOP_DURATION) {
            // 停车完成
            isStopping = false;
            N1 = 0;
            temperature = T0;  // 回到环境温度
            return;
        }

        // 使用对数衰减
        const double a = 0.9;  // 衰减系数

        // 计算当前值相对于停车开始时的衰减比例
        double decayRatio = pow(a, elapsedStopTime);

        // 更新转速和温度
        N1 = N1 * decayRatio;
        temperature = T0 + (temperature - T0) * decayRatio;

        // 更新传感器显示值
        double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;
        double displaySpeed = (N1 / 100.0) * RATED_SPEED * randFactor;

        speedSensor1->setValue(displaySpeed);
        speedSensor2->setValue(displaySpeed);
        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // 调试输出
        std::cout << "Stop Phase - Time: " << elapsedStopTime
            << "s, N1: " << N1
            << "%, Temp: " << temperature << std::endl;
    }

    void checkWarnings(double currentTime) {
        // 检查传感效性
        if (!speedSensor1->getValidity() && speedSensor2->getValidity()) {
            addWarning("Speed Sensor 1 Failure", NORMAL, currentTime);
        }

        if (!speedSensor1->getValidity() && !speedSensor2->getValidity()) {
            addWarning("Both Speed Sensors Failed - Engine Shutdown", WARNING, currentTime);
            stop();
        }

        // 检查转速超限
        double n1Percent = getN1();
        if (n1Percent > 120) {
            addWarning("N1 Overspeed Level 2 - Engine Shutdown", WARNING, currentTime);
            stop();
        }
        else if (n1Percent > 105) {
            addWarning("N1 Overspeed Level 1", CAUTION, currentTime);
        }

        // 检查温度超限
        if (isStarting) {
            if (temperature > 1000) {
                addWarning("EGT Overtemp Level 2 During Start - Engine Shutdown", WARNING, currentTime);
                stop();
            }
            else if (temperature > 850) {
                addWarning("EGT Overtemp Level 1 During Start", CAUTION, currentTime);
            }
        }
        else if (isRunning) {
            if (temperature > 1100) {
                addWarning("EGT Overtemp Level 2 - Engine Shutdown", WARNING, currentTime);
                stop();
            }
            else if (temperature > 950) {
                addWarning("EGT Overtemp Level 1", CAUTION, currentTime);
            }
        }

        // 检查燃油系统
        if (fuelAmount < 1000 && isRunning) {
            addWarning("Low Fuel Level", CAUTION, currentTime);
        }

        if (fuelFlow > 50) {
            addWarning("Fuel Flow Exceeded Limit", CAUTION, currentTime);
        }
    }

    const vector<WarningMessage>& getWarnings() const {
        return warnings;
    }

    void increaseThrust() {
        if (!isRunning || isStarting || isStopping) {
            cout << "Not in running state" << endl;
            return;  // 只在稳定运行状态下允许调节推力
        }
        isThrusting = true;

        // 增加燃油流量
        fuelFlow += THRUST_FUEL_STEP;

        // 生成3%~5%之间的随机变化率
        double changeRatio = THRUST_PARAM_MIN_CHANGE +
            (static_cast<double>(rand()) / RAND_MAX) *
            (THRUST_PARAM_MAX_CHANGE - THRUST_PARAM_MIN_CHANGE);

        // 更新转速和温度
        double baseSpeed = RATED_SPEED * N1 / 100.0;  // 基准转速
        double newSpeed = baseSpeed * (1.0 + changeRatio);
        N1 = (newSpeed / RATED_SPEED) * 100.0;


        // 限制N1不超过100%
        if (N1 > 125.0) {
            N1 = 125.0;
            newSpeed = RATED_SPEED;
        }

        // 更新传感器显示值
        speedSensor1->setValue(newSpeed);
        speedSensor2->setValue(newSpeed);

        // 更新温度
        temperature = temperature * (1.0 + changeRatio);

        // 更新EGT传感器
        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // 调试输出
        std::cout << "Thrust increased - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow << std::endl;
    }

    void decreaseThrust() {
        if (!isRunning || isStarting || isStopping) {
            return;  // 只在稳定运行状态下允许调节推力
        }
        isThrusting = true;

        // 确保燃油流量不会低于最小值
        if (fuelFlow > THRUST_FUEL_STEP) {
            fuelFlow -= THRUST_FUEL_STEP;
        }

        // 生成3%~5%之间的随机变化率
        double changeRatio = THRUST_PARAM_MIN_CHANGE +
            (static_cast<double>(rand()) / RAND_MAX) *
            (THRUST_PARAM_MAX_CHANGE - THRUST_PARAM_MIN_CHANGE);

        // 更新转速和温度
        double baseSpeed = RATED_SPEED * N1 / 100.0;  // 基准转速
        double newSpeed = baseSpeed * (1.0 - changeRatio);
        N1 = (newSpeed / RATED_SPEED) * 100.0;

        // 限制N1不低于最小值（比如60%）
        if (N1 < 0) {
            N1 = 0;
            newSpeed = 0;
        }

        // 更新传感器显示值
        speedSensor1->setValue(newSpeed);
        speedSensor2->setValue(newSpeed);

        // 更新温度，基准温度850度
        temperature = temperature * (1.0 - changeRatio);

        // 更新EGT传感器
        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // 调试输出
        std::cout << "Thrust decreased - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow << std::endl;
    }

    // 添加传感器故障模拟方法
    void simulateSensorFailure(int sensorType) {
        switch (sensorType) {
        case 0: // 单个转速传感器故障
            speedSensor1->setValidity(false);
            break;
        case 1: // 单发转速传感器故障
            speedSensor1->setValidity(false);
            speedSensor2->setValidity(false);
            break;
        case 2: // 单个EGT传感器故障
            egtSensor1->setValidity(false);
            break;
        case 3: // 单发EGT传感器故障
            egtSensor1->setValidity(false);
            egtSensor2->setValidity(false);
            break;
            // ... 其他障???拟
        }
    }

    double getCurrentTime() const {
        return static_cast<double>(GetTickCount64()) / 1000.0;
    }
};

const double Engine::RATED_SPEED = 40000.0;
const double Engine::T0 = 20.0;
const double Engine::STOP_DURATION = 10.0;  // 10秒停车时间

// GUI类
class GUI {
private:
    static const int WINDOW_WIDTH = 1600;    // 增加窗口宽度
    static const int WINDOW_HEIGHT = 900;    // 增加窗口高度
    static const int DIAL_RADIUS = 60;       // 调整表盘大小
    static const int DIAL_SPACING_X = 180;   // 调整表盘水平间
    static const int DIAL_SPACING_Y = 180;   // 增加垂直间距，从140改为180
    static const int DIAL_START_X = 100;     // 调整表盘起始位置
    static const int DIAL_START_Y = 150;
    static const int WARNING_BOX_WIDTH = 380;  // 调整警告框宽度
    static const int WARNING_BOX_HEIGHT = 30;
    static const int WARNING_START_X = 750;
    static const int WARNING_START_Y = 100;
    static const int WARNING_SPACING = 5;
    static const int REFRESH_RATE = 200;  // 200Hz = 5ms刷新一次
    static const int FRAME_TIME = 1000 / REFRESH_RATE;  // 5ms
    ULONGLONG lastFrameTime;
    IMAGE* backBuffer;
    Engine* engine;  // 确保 engine 指针被正确声明

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

    GUI(int w, int h, Engine* eng) : engine(eng) {
        // 确保 engine 指针有效
        if (!eng) {
            throw std::runtime_error("Invalid engine pointer");
        }

        // 首先初始化按钮，这样在预热时就能使用
        startButton = { 50, 500, 120, 40, _T("START"), false, RGB(0, 150, 0), WHITE };
        stopButton = { 200, 500, 120, 40, _T("STOP"), false, RGB(150, 0, 0), WHITE };
        increaseThrust = { 350, 500, 120, 40, _T("THRUST+"), false, RGB(0, 100, 150), WHITE };
        decreaseThrust = { 500, 500, 120, 40, _T("THRUST-"), false, RGB(0, 100, 150), WHITE };

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

        // 预热绘图系统
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
        // 绘制完整的圆形底盘
        setcolor(RGB(60, 60, 60));
        setfillcolor(RGB(20, 20, 20));
        fillcircle(x, y, radius);

        // 对于转速表盘特殊处理
        bool isN1Dial = _tcsstr(label, _T("N1")) != nullptr;
        double displayValue = value;
        double displayMaxValue = maxValue;

        if (isN1Dial) {
            displayValue = (value / 40000.0) * 100;  // 40000是额定转速
            displayMaxValue = 125;  // 最大125%
        }

        // 绘制刻度 - 对于转速表盘只绘制0-210度的刻度
        int startAngle = 90;  // 起始角度（12点钟位置）
        int totalArcDegrees = isN1Dial ? 210 : 360;  // N1表盘210度，其??360度

        // 绘制主刻度线
        for (int i = 0; i <= totalArcDegrees; i += 30) {
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
        for (int i = 0; i <= totalArcDegrees; i += 6) {
            if (i % 30 != 0) {  // 跳过主刻度位
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

        // 根据值的状态选择颜色
        COLORREF valueColor = color;
        if (isN1Dial) {
            if (displayValue > 105) valueColor = RGB(255, 128, 0);
            if (displayValue > 120) valueColor = RGB(255, 0, 0);
        }
        else {
            if (displayValue > maxValue * 0.9) valueColor = RGB(255, 128, 0);
            if (displayValue > maxValue * 0.95) valueColor = RGB(255, 0, 0);
        }

        // 绘制值的弧形背景
        double angleRatio = isN1Dial ? (displayValue / displayMaxValue) : (displayValue / maxValue);
        angleRatio = min(angleRatio, 1.0);
        double angle = angleRatio * (isN1Dial ? 210 : 360);

        // 使用渐变色绘制弧形
        setfillcolor(valueColor);
        setcolor(valueColor);
        POINT* points = new POINT[362];
        points[0].x = x;
        points[0].y = y;

        for (int i = 0; i <= angle; i++) {
            double rad = (90 - i) * PI / 180;
            points[i + 1].x = x + (radius - 15) * cos(rad);  // 减小填充半径，留出刻度空间
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

        // 绘制中心装饰
        setfillcolor(RGB(40, 40, 40));
        fillcircle(x, y, 8);
        setfillcolor(valueColor);
        fillcircle(x, y, 4);

        // 绘制标签和数值
        settextstyle(20, 0, _T("Arial"));
        TCHAR valueText[32];
        if (isN1Dial) {
            _stprintf_s(valueText, _T("%s: %.1f%%"), label, displayValue);
        }
        else {
            _stprintf_s(valueText, _T("%s: %.1f"), label, value);
        }
        outtextxy(x - 40, y + radius + 10, valueText);
    }

    void drawDials() {
        // 第一行 - 左发动机传感器
        drawArcDial(DIAL_START_X, DIAL_START_Y, DIAL_RADIUS,
            engine->getSpeedSensor1()->getValue(), 40000, _T("N1-L1"), RGB(0, 255, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X, DIAL_START_Y, DIAL_RADIUS,
            engine->getSpeedSensor2()->getValue(), 40000, _T("N1-L2"), RGB(0, 255, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X * 2, DIAL_START_Y, DIAL_RADIUS,
            engine->getEgtSensor1()->getValue(), 1200, _T("EGT-L1"), RGB(255, 128, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X * 3, DIAL_START_Y, DIAL_RADIUS,
            engine->getEgtSensor2()->getValue(), 1200, _T("EGT-L2"), RGB(255, 128, 0));

        // 第二行 - 右发动机传感器 (增加了垂直间距)
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
        // 绘制按钮背景
        setfillcolor(btn.pressed ? RGB(150, 150, 150) : btn.bgColor);
        solidroundrect(btn.x, btn.y, btn.x + btn.width, btn.y + btn.height, 10, 10);

        // 绘制按钮字
        settextstyle(20, 0, _T("Arial"));
        setbkmode(TRANSPARENT);
        settextcolor(btn.textColor);

        // 计算文字位置使其居中
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
        case CAUTION: boxColor = RGB(255, 128, 0); break;
        case WARNING: boxColor = RGB(255, 0, 0); break;
        default: boxColor = RGB(128, 128, 128);
        }

        // 绘制警告框边框效果
        // 外边框
        setfillcolor(RGB(30, 30, 30));
        solidrectangle(x - 2, y - 2, x + WARNING_BOX_WIDTH + 2, y + WARNING_BOX_HEIGHT + 2);

        // 内部填充
        setfillcolor(isActive ? RGB(60, 60, 60) : RGB(40, 40, 40));
        solidrectangle(x, y, x + WARNING_BOX_WIDTH, y + WARNING_BOX_HEIGHT);

        // 绘制边框高光
        setcolor(isActive ? boxColor : RGB(80, 80, 80));
        rectangle(x - 1, y - 1, x + WARNING_BOX_WIDTH + 1, y + WARNING_BOX_HEIGHT + 1);

        // 绘制警告文本
        settextstyle(16, 0, _T("Arial"));
        settextcolor(isActive ? boxColor : RGB(128, 128, 128));
        wstring wstr(message.begin(), message.end());
        outtextxy(x + 10, y + 5, wstr.c_str());

        // 如果警告激活，添加左侧指示条
        if (isActive) {
            setfillcolor(boxColor);
            solidrectangle(x, y, x + 3, y + WARNING_BOX_HEIGHT);
        }
    }

    void drawAllWarningBoxes() {
        int x = WARNING_START_X;
        int y = WARNING_START_Y;
        const vector<WarningMessage>& warnings = engine->getWarnings();

        // 定义所有可能的警??
        struct WarningDef {
            string message;
            WarningLevel level;
        };

        WarningDef allWarnings[] = {
            {"Speed Sensor 1 Failure", NORMAL},
            {"Single Engine Speed Sensor Failure", CAUTION},
            {"EGT Sensor 1 Failure", NORMAL},
            {"Single Engine EGT Sensor Failure", CAUTION},
            {"Dual Engine Sensor Failure - Shutdown", WARNING},
            {"Low Fuel Level", CAUTION},
            {"Fuel Sensor Failure", WARNING},
            {"Fuel Flow Exceeded Limit", CAUTION},
            {"N1 Overspeed Level 1", CAUTION},
            {"N1 Overspeed Level 2 - Shutdown", WARNING},
            {"EGT Overtemp Level 1 During Start", CAUTION},
            {"EGT Overtemp Level 2 During Start - Shutdown", WARNING},
            {"EGT Overtemp Level 1 Stable", CAUTION},
            {"EGT Overtemp Level 2 Stable - Shutdown"}
        };

        // 绘制所有警告框
        for (int i = 0; i < 14; i++) {
            bool isActive = false;
            // 检查当前警告否激活
            for (const auto& warning : warnings) {
                if (warning.message == allWarnings[i].message &&
                    warning.timestamp >= engine->getCurrentTime() - 5.0) {
                    isActive = true;
                    break;
                }
            }

            drawWarningBox(allWarnings[i].message, allWarnings[i].level, isActive, x, y);
            y += WARNING_BOX_HEIGHT + WARNING_SPACING;

            // 每7个警告换列
            if (i == 6) {
                x += WARNING_BOX_WIDTH + WARNING_SPACING;
                y = WARNING_START_Y;
            }
        }
    }

    void drawFuelFlow() {
        double fuelFlow = engine->getFuelFlow();

        // 根据燃油流速决定颜色
        COLORREF textColor, borderColor, bgColor;
        if (fuelFlow > 0) {
            // 活跃状态 - 使用琥珀色
            textColor = RGB(255, 180, 0);    // 明亮的琥珀色文字
            borderColor = RGB(255, 140, 0);   // 深琥珀色边框
            bgColor = RGB(80, 60, 20);       // 暗琥珀色背景
        }
        else {
            // 非活跃状态 - 使用灰色
            textColor = RGB(150, 150, 150);   // 灰色文字
            borderColor = RGB(100, 100, 100); // 深灰色边框
            bgColor = RGB(40, 40, 40);       // 暗灰色背景
        }

        // 设置文本样式
        settextstyle(24, 0, _T("Arial"));
        settextcolor(textColor);

        // 创建显示文本
        TCHAR flowText[64];
        _stprintf_s(flowText, _T("Fuel Flow: %.1f kg/h"), fuelFlow);

        // ???算显示位置
        int x = 50;
        int y = 560;
        int width = 200;
        int height = 30;

        // 绘制外边框阴影效果
        setfillcolor(RGB(30, 30, 30));
        solidrectangle(x - 1, y - 1, x + width + 1, y + height + 1);

        // 绘制主背景
        setfillcolor(bgColor);
        solidrectangle(x, y, x + width, y + height);

        // 绘制边框
        setcolor(borderColor);
        rectangle(x, y, x + width, y + height);

        // 如果处于活跃状态，添加左侧指示条
        if (fuelFlow > 0) {
            setfillcolor(borderColor);
            solidrectangle(x, y, x + 3, y + height);
        }

        // 绘制文本（稍微向右偏移以避开左侧指示条）
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
        drawFuelFlow();  // 确保在其他元素之后绘制

        SetWorkingImage(NULL);
        putimage(0, 0, backBuffer);

        lastFrameTime = currentTime;
    }

    bool checkButtonClick(const Button& btn, int mouseX, int mouseY) {
        return mouseX >= btn.x && mouseX <= btn.x + btn.width &&
            mouseY >= btn.y && mouseY <= btn.y + btn.height;
    }
};

// 数据记录类
class DataLogger {
private:
    ofstream dataFile;
    ofstream logFile;

public:
    DataLogger() {
        time_t now = time(0);
        unsigned long long timestamp = static_cast<unsigned long long>(now);
        string timestampStr = to_string(timestamp);
        dataFile.open("data_" + timestampStr + ".csv");
        logFile.open("log_" + timestampStr + ".txt");

        dataFile << "Time,N1,EGT,Fuel Flow,Fuel Amount\n";
    }

    void logData(double time, double n1, double egt, double fuelFlow, double fuelAmount) {
        dataFile << time << "," << n1 << "," << egt << ","
            << fuelFlow << "," << fuelAmount << "\n";
    }

    void logWarning(double time, const string& warning) {
        logFile << static_cast<long long>(time) << ": " << warning << "\n";
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
        const double fixedTimeStep = 1.0 / 200.0;  // 同样使用200Hz的更新频率
        double accumulator = 0.0;

        bool running = true;

        // 预热循环
        for (int i = 0; i < 100; i++) {
            engine.update(fixedTimeStep);
        }

        while (running) {
            currentTime = GetTickCount64();
            double deltaTime = (currentTime - lastUpdateTime) / 1000.0;  // 改名为deltaTime
            if (deltaTime > 0.25) deltaTime = 0.25;  // 防止过大的时间步长
            lastUpdateTime = currentTime;

            accumulator += deltaTime;

            // 处理输入
            while (MouseHit()) {
                MOUSEMSG msg = GetMouseMsg();
                if (msg.uMsg == WM_LBUTTONDOWN) {
                    if (gui.checkButtonClick(gui.startButton, msg.x, msg.y)) {
                        engine.start();
                    }
                    else if (gui.checkButtonClick(gui.stopButton, msg.x, msg.y)) {
                        engine.stop();
                    }
                    else if (gui.checkButtonClick(gui.increaseThrust, msg.x, msg.y)) {
                        engine.increaseThrust();
                    }
                    else if (gui.checkButtonClick(gui.decreaseThrust, msg.x, msg.y)) {
                        engine.decreaseThrust();
                    }
                }
            }

            // 物理更新也使用5ms的固定时间步长
            while (accumulator >= fixedTimeStep) {
                engine.update(fixedTimeStep);
                engine.checkWarnings(currentTime / 1000.0);
                accumulator -= fixedTimeStep;
            }

            gui.update();

            if (GetAsyncKeyState(VK_ESCAPE)) {
                running = false;
            }

            // 精确控制循环时间
            ULONGLONG endTime = GetTickCount64();
            ULONGLONG elapsedTime = endTime - currentTime;  // 改名为elapsedTime
            if (elapsedTime < 5) {  // 如果一帧用时少于5ms
                Sleep(5 - elapsedTime);  // 等待剩余时间
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