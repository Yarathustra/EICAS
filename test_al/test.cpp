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
#include <iomanip>  // Ϊsetprecision��fixed����ͷ�ļ�
#include <map>
#include "DataLogger.h"  // 添加头文件包含
using namespace std;

#define PI 3.14159265358979323846

// ļͷӸ澯ϵͳض
enum WarningLevel {
    NORMAL,     // ɫ
    CAUTION,    // ɫ
    WARNING,    // ɫ
    INVALID     // Ч
};

struct WarningMessage {
    string message;
    WarningLevel level;
    double timestamp;

    WarningMessage(const string& msg, WarningLevel lvl, double time)
        : message(msg), level(lvl), timestamp(time) {}
};

// ����������
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

// Ȼ Engine ȷʹ DataLogger
class Engine {
private:
    Sensor* speedSensor1;
    Sensor* speedSensor2;
    Sensor* egtSensor1;
    Sensor* egtSensor2;
    Sensor* fuelSensor;  // ȼʹ
    double fuelFlow;
    double fuelAmount;
    bool isRunning;
    bool isStarting;
    bool isStopping;  // ͣ״̬
    bool isThrusting;  // 仯״̬
    double stopStartTime;  // ʼͣʱ
    static const double RATED_SPEED; // ??ת
    static const double T0;  // ʼ
    static const double STOP_DURATION;  // ͣʱ䣨10

    // еĳ
    double N1;        // ת
    double temperature; // ¶
    double prevFuelAmount; // һʱȼ
    double timeStep;      // ʱ䲽
    vector<WarningMessage> warnings;
    double lastWarningTime;
    double accumulatedTime;  // ۻʱ
    const double THRUST_FUEL_STEP = 1.0;  // ÿ仯ȼ
    const double THRUST_PARAM_MIN_CHANGE = 0.03;  // С仯 3%
    const double THRUST_PARAM_MAX_CHANGE = 0.05;  // 仯 5%
    DataLogger* logger;  // ־¼ָ
    map<string, double> lastLogTimes;  // ÿ־¼ʱ

    void addWarning(const string& message, WarningLevel level, double currentTime) {
        // ǷҪµľ棨5ڲظ
        if (currentTime - lastWarningTime >= 5.0 || warnings.empty() ||
            warnings.back().message != message) {
            warnings.push_back(WarningMessage(message, level, currentTime));
            lastWarningTime = currentTime;

            // ǷҪ¼־5ڲظ¼ͬһ
            auto lastLogIt = lastLogTimes.find(message);
            if (lastLogIt == lastLogTimes.end() ||
                (currentTime - lastLogIt->second >= 5.0)) {
                // ¼¼ log
                string levelStr;
                switch (level) {
                case NORMAL: levelStr = "NORMAL"; break;
                case CAUTION: levelStr = "CAUTION"; break;
                case WARNING: levelStr = "WARNING"; break;
                default: levelStr = "INVALID"; break;
                }

                // ʽ
                string logMessage = message + " [" + levelStr + "]";
                if (logger) {  // ȫ
                    logger->logWarning(currentTime, logMessage);
                }

                // ¸¸
                lastLogTimes[message] = currentTime;
            }
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
        isStopping = true;  // ͣ
        stopStartTime = accumulatedTime;  // ¼ͣʼʱ
        fuelFlow = 0;  // ȼֱ
    }

    void updateFuelFlow() {
        if (fuelFlow > 0) {
            fuelAmount -= fuelFlow * timeStep;
            if (fuelAmount < 0) fuelAmount = 0;

            // ȼ
            fuelSensor->setValue(fuelAmount);
        }
    }
    Engine() {
        speedSensor1 = new Sensor();
        speedSensor2 = new Sensor();
        egtSensor1 = new Sensor();
        egtSensor2 = new Sensor();
        fuelSensor = new Sensor();  // ʼȼ
        fuelFlow = 0;
        fuelAmount = 20000;
        isRunning = false;
        isStarting = false;
        lastWarningTime = 0;

        // ʼ
        N1 = 0;
        temperature = T0;  // T0
        prevFuelAmount = fuelAmount;
        timeStep = 0.005;  // 5ms
        accumulatedTime = 0.0;  // ʼۻʱ
        isStopping = false;
        stopStartTime = 0;
        isThrusting = false;  // ʼ

        // ʼEGT
        egtSensor1->setValue(T0);
        egtSensor2->setValue(T0);
        logger = new DataLogger();  // ־¼
    }

    Sensor* getSpeedSensor1() { return speedSensor1; }

    void updateStartPhase(double timeStep) {
        isThrusting = false;
        // ۻʱ
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
            temperature = T0;  // 2¶

            egtSensor1->setValue(temperature);
            egtSensor2->setValue(temperature);

            isStarting = true;
        }
        else {
            // 2
            double t = accumulatedTime - 2.0;  // t0ʼʱ
            double speed = 20000.0 + 23000.0 * log10(1.0 + t);
            N1 = (speed / RATED_SPEED) * 100.0;

            speedSensor1->setValue(speed);
            speedSensor2->setValue(speed);
            fuelFlow = 10.0 + 42.0 * log10(1.0 + t);

            // 㹫 T = 900*lg(t-1) + T0
            temperature = 900.0 * log10(t + 1.0) + T0;

            double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;
            double displaySpeed = speed * randFactor;

            speedSensor1->setValue(displaySpeed);
            speedSensor2->setValue(displaySpeed);
            temperature *= randFactor;

            egtSensor1->setValue(temperature);
            egtSensor2->setValue(temperature);

            // N195%ת
            if (N1 >= 95.0) {
                isStarting = false;
                isRunning = true;
                std::cout << "Reached running state" << std::endl;
            }
        }

        updateFuelFlow();
    }

    void start() {
        isRunning = false;  // �false
        isStarting = true;
        isStopping = false;  // ͣ
        accumulatedTime = 0.0;  // ۻʱ
    }

    void update(double timeStep) {
        accumulatedTime += timeStep;

        if (isStarting) {
            updateStartPhase(timeStep);
        }
        else if (isRunning) {
            // ȼ
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
        // ���н׶Σڡ%
        double randFactor = 1.0 + (rand() % 6 - 3) / 100.0;  // 0.971.03

        // ת
        double baseSpeed = RATED_SPEED * 0.95;  // 95%
        double speed = baseSpeed * randFactor;
        N1 = (speed / RATED_SPEED) * 100.0;

        // 
        speedSensor1->setValue(speed);
        speedSensor2->setValue(speed);

        // ¶ - 
        double targetTemp = 850.0;  // ̬
        double tempDiff = targetTemp - temperature;
        double tempAdjustRate = 0.1;  // 

        // 
        temperature += tempDiff * tempAdjustRate;
        // 
        temperature *= randFactor;

        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // ȼ
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

        // ȼ
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
            // ͣ
            isStopping = false;
            N1 = 0;
            temperature = T0;  // 
            return;
        }

        // 
        const double a = 0.9;  // 

        // 
        double decayRatio = pow(a, elapsedStopTime);

        // ת
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
        // ��Ч
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

        // ˫�����������ϼ��
        if ((!speedSensor1->getValidity() && !speedSensor2->getValidity()) ||
            (!egtSensor1->getValidity() && !egtSensor2->getValidity())) {
            addWarning("Dual Engine Sensor Failure - Shutdown", WARNING, currentTime);
            stop();
        }

        // ȼ��ϵͳ���
        if (fuelAmount < 1000 && isRunning) {
            addWarning("Low Fuel Level", CAUTION, currentTime);
        }

        if (!fuelSensor->getValidity()) {
            addWarning("Fuel Sensor Failure", WARNING, currentTime);
        }

        if (fuelFlow > 50) {
            addWarning("Fuel Flow Exceeded Limit", CAUTION, currentTime);
        }

        // ת�ټ��
        double n1Percent = getN1();
        if (n1Percent > 120) {
            addWarning("N1 Overspeed Level 2 - Engine Shutdown", WARNING, currentTime);
            stop();
        }
        else if (n1Percent > 105) {
            addWarning("N1 Overspeed Level 1", CAUTION, currentTime);
        }

        // �¶ȼ��
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
            return;  // ֻ���ȶ�����״̬��������������
        }
        isThrusting = true;

        // ����ȼ������
        fuelFlow += THRUST_FUEL_STEP;

        // ����3%~5%֮�������仯��
        double changeRatio = THRUST_PARAM_MIN_CHANGE +
            (static_cast<double>(rand()) / RAND_MAX) *
            (THRUST_PARAM_MAX_CHANGE - THRUST_PARAM_MIN_CHANGE);

        // ����ת�ٺ��¶�
        double baseSpeed = RATED_SPEED * N1 / 100.0;  // ��׼ת��
        double newSpeed = baseSpeed * (1.0 + changeRatio);
        N1 = (newSpeed / RATED_SPEED) * 100.0;


        // ����N1������100%
        if (N1 > 125.0) {
            N1 = 125.0;
            newSpeed = RATED_SPEED;
        }

        // ���´�������ʾֵ
        speedSensor1->setValue(newSpeed);
        speedSensor2->setValue(newSpeed);

        // �����¶�
        temperature = temperature * (1.0 + changeRatio);

        // ����EGT������
        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // �������
        std::cout << "Thrust increased - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow << std::endl;
    }

    void decreaseThrust() {
        if (!isRunning || isStarting || isStopping) {
            return;  // ֻ���ȶ�����״̬��������������
        }
        isThrusting = true;

        // ȷ��ȼ���������������Сֵ
        if (fuelFlow > THRUST_FUEL_STEP) {
            fuelFlow -= THRUST_FUEL_STEP;
        }

        // ����3%~5%֮�������仯��
        double changeRatio = THRUST_PARAM_MIN_CHANGE +
            (static_cast<double>(rand()) / RAND_MAX) *
            (THRUST_PARAM_MAX_CHANGE - THRUST_PARAM_MIN_CHANGE);

        // ����ת�ٺ��¶�
        double baseSpeed = RATED_SPEED * N1 / 100.0;  // ��׼ת��
        double newSpeed = baseSpeed * (1.0 - changeRatio);
        N1 = (newSpeed / RATED_SPEED) * 100.0;

        // ����N1��������Сֵ������60%��
        if (N1 < 0) {
            N1 = 0;
            newSpeed = 0;
        }

        // ���´�������ʾֵ
        speedSensor1->setValue(newSpeed);
        speedSensor2->setValue(newSpeed);

        // �����¶ȣ���׼�¶�850��
        temperature = temperature * (1.0 - changeRatio);

        // ����EGT������
        egtSensor1->setValue(temperature);
        egtSensor2->setValue(temperature);

        // �������
        std::cout << "Thrust decreased - N1: " << N1
            << "%, Temp: " << temperature
            << ", Fuel Flow: " << fuelFlow << std::endl;
    }

    // ���Ӵ���������ģ�ⷽ��
    void simulateSensorFailure(int sensorType) {
        switch (sensorType) {
        case 0: // ����ת�ٴ���������
            speedSensor1->setValidity(false);
            break;
        case 1: // ����ת�ٴ���������
            speedSensor1->setValidity(false);
            speedSensor2->setValidity(false);
            break;
        case 2: // ����EGT����������
            egtSensor1->setValidity(false);
            break;
        case 3: // ����EGT����������
            egtSensor1->setValidity(false);
            egtSensor2->setValidity(false);
            break;
            // ... ��������
        }
    }

    double getCurrentTime() const {
        return static_cast<double>(GetTickCount64()) / 1000.0;
    }

    // ����ȼ�ʹ������ķ��ʷ���
    Sensor* getFuelSensor() { return fuelSensor; }

    // ����������ɾ��ȼ�ʹ�����
    ~Engine() {
        delete speedSensor1;
        delete speedSensor2;
        delete egtSensor1;
        delete egtSensor2;
        delete fuelSensor;  // ɾ��ȼ�ʹ�����
        if (logger) {
            delete logger;  // ��ȫɾ����־��¼��
            logger = nullptr;
        }
    }
};

const double Engine::RATED_SPEED = 40000.0;
const double Engine::T0 = 20.0;
const double Engine::STOP_DURATION = 10.0;  // 10��ͣ��ʱ��

// GUI��
class GUI {
private:
    static const int WINDOW_WIDTH = 1600;    // ���Ӵ��ڿ���
    static const int WINDOW_HEIGHT = 900;    // ���Ӵ��ڸ߶�
    static const int DIAL_RADIUS = 60;       // �������̴�С
    static const int DIAL_SPACING_X = 180;   // ��������ˮƽ��
    static const int DIAL_SPACING_Y = 180;   // ���Ӵ�ֱ��࣬��140��Ϊ180
    static const int DIAL_START_X = 100;     // ����������ʼλ��
    static const int DIAL_START_Y = 150;
    static const int WARNING_BOX_WIDTH = 380;  // ������������
    static const int WARNING_BOX_HEIGHT = 30;
    static const int WARNING_START_X = 750;
    static const int WARNING_START_Y = 100;
    static const int WARNING_SPACING = 5;
    static const int REFRESH_RATE = 200;  // 200Hz = 5msˢ��һ��
    static const int FRAME_TIME = 1000 / REFRESH_RATE;  // 5ms
    ULONGLONG lastFrameTime;
    IMAGE* backBuffer;
    Engine* engine;  // ȷ�� engine ָ�뱻��ȷ����

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
        // ȷ�� engine ָ����Ч
        if (!eng) {
            throw std::runtime_error("Invalid engine pointer");
        }

        // ���ȳ�ʼ����ť��������Ԥ��ʱ����ʹ��
        startButton = { 50, 500, 120, 40, _T("START"), false, RGB(0, 150, 0), WHITE };
        stopButton = { 200, 500, 120, 40, _T("STOP"), false, RGB(150, 0, 0), WHITE };
        increaseThrust = { 350, 500, 120, 40, _T("THRUST+"), false, RGB(0, 100, 150), WHITE };
        decreaseThrust = { 500, 500, 120, 40, _T("THRUST-"), false, RGB(0, 100, 150), WHITE };

        // ��ʼ��ͼ��ϵͳ
        initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
        setbkcolor(RGB(40, 40, 40));
        cleardevice();

        // �����󱸻�����
        backBuffer = new IMAGE(WINDOW_WIDTH, WINDOW_HEIGHT);
        if (!backBuffer) {
            throw std::runtime_error("Failed to create back buffer");
        }

        SetWorkingImage(backBuffer);
        cleardevice();
        SetWorkingImage(NULL);

        // Ԥ�Ȼ�ͼϵͳ
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
        // ����������Բ�ε���
        setcolor(RGB(60, 60, 60));
        setfillcolor(RGB(20, 20, 20));
        fillcircle(x, y, radius);

        // ����ת�ٱ��̺�EGT���̵����⴦��
        bool isN1Dial = _tcsstr(label, _T("N1")) != nullptr;
        bool isEGTDial = _tcsstr(label, _T("EGT")) != nullptr;
        double valueToDisplay = value;
        double maxValueToDisplay = maxValue;
        bool isStarting = engine->getIsStarting();
        int arcDegrees = 360;  // Ĭ��Ϊ360��

        if (isN1Dial) {
            valueToDisplay = (value / 40000.0) * 100;  // 40000�Ƕת��
            maxValueToDisplay = 125;  // ���125%
            arcDegrees = 210;  // N1����ʹ��210��
        }
        else if (isEGTDial) {
            maxValueToDisplay = 1200;  // EGT���ֵ1200��
            arcDegrees = 210;   // EGT����Ҳʹ��210�Ȼ���
        }

        // ���ƿ̶�
        int startAngle = 90;  // ��ʼ�Ƕȣ�12����λ�ã�

        // �������̶���
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

        // ���ƴο̶���
        for (int i = 0; i <= arcDegrees; i += 6) {
            if (i % 30 != 0) {  // �������̶�λ
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

        // ����ֵ��״̬ѡ����ɫ
        COLORREF valueColor = RGB(255, 255, 255);  // Ĭ�ϰ�ɫ
        if (isN1Dial) {
            if (valueToDisplay > 105) valueColor = RGB(255, 128, 0);
            if (valueToDisplay > 120) valueColor = RGB(255, 0, 0);
        }
        else if (isEGTDial) {
            if (isStarting) {
                // ���������е��¶Ⱦ���
                if (valueToDisplay > 1000) {
                    valueColor = RGB(255, 0, 0);      // ��ɫ���棨����2��
                }
                else if (valueToDisplay > 850) {
                    valueColor = RGB(255, 128, 0);    // ����ɫ���棨����1��
                }
            }
            else {
                // ��̬����ʱ���¶Ⱦ���
                if (valueToDisplay > 1100) {
                    valueColor = RGB(255, 0, 0);      // ��ɫ���棨����4��
                }
                else if (valueToDisplay > 950) {
                    valueColor = RGB(255, 128, 0);    // ����ɫ���棨����3��
                }
            }
        }

        // ����ֵ�Ļ��α���
        double angleRatio = isN1Dial ? (valueToDisplay / maxValueToDisplay) : (valueToDisplay / maxValue);
        angleRatio = min(angleRatio, 1.0);
        double angle = angleRatio * arcDegrees;

        // ʹ�ý���ɫ���ƻ���
        setfillcolor(valueColor);
        setcolor(valueColor);
        POINT* points = new POINT[362];
        points[0].x = x;
        points[0].y = y;

        for (int i = 0; i <= angle; i++) {
            double rad = (90 - i) * PI / 180;
            points[i + 1].x = x + (radius - 15) * cos(rad);  // ��С���뾶�������̶ȿռ�
            points[i + 1].y = y - (radius - 15) * sin(rad);
        }

        solidpolygon(points, static_cast<int>(angle) + 2);
        delete[] points;

        // ����ָ��
        double pointerAngle = (90 - angle) * PI / 180;
        int pointerLength = radius - 10;
        int pointerX = x + pointerLength * cos(pointerAngle);
        int pointerY = y - pointerLength * sin(pointerAngle);

        // ����ָ����Ӱ
        setcolor(RGB(20, 20, 20));
        setlinestyle(PS_SOLID, 3);
        line(x, y, pointerX + 2, pointerY + 2);

        // ����ָ�뱾��
        setcolor(WHITE);
        setlinestyle(PS_SOLID, 2);
        line(x, y, pointerX, pointerY);

        // ��������װ��
        setfillcolor(RGB(40, 40, 40));
        fillcircle(x, y, 8);
        setfillcolor(valueColor);
        fillcircle(x, y, 4);

        // ���Ʊ�ǩ����ֵ
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
        // ��һ�� - �󷢶���������
        drawArcDial(DIAL_START_X, DIAL_START_Y, DIAL_RADIUS,
            engine->getSpeedSensor1()->getValue(), 40000, _T("N1-L1"), RGB(0, 255, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X, DIAL_START_Y, DIAL_RADIUS,
            engine->getSpeedSensor2()->getValue(), 40000, _T("N1-L2"), RGB(0, 255, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X * 2, DIAL_START_Y, DIAL_RADIUS,
            engine->getEgtSensor1()->getValue(), 1200, _T("EGT-L1"), RGB(255, 128, 0));
        drawArcDial(DIAL_START_X + DIAL_SPACING_X * 3, DIAL_START_Y, DIAL_RADIUS,
            engine->getEgtSensor2()->getValue(), 1200, _T("EGT-L2"), RGB(255, 128, 0));

        // �ڶ��� - �ҷ���������?? (�����˴�ֱ���)
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
        // ���ư�ť����
        setfillcolor(btn.pressed ? RGB(150, 150, 150) : btn.bgColor);
        solidroundrect(btn.x, btn.y, btn.x + btn.width, btn.y + btn.height, 10, 10);

        // ���ư�ť��
        settextstyle(20, 0, _T("Arial"));
        setbkmode(TRANSPARENT);
        settextcolor(btn.textColor);

        // ��������λ��ʹ�����
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
        case CAUTION: boxColor = RGB(255, 128, 0); break;  // ����ɫ
        case WARNING: boxColor = RGB(255, 0, 0); break;    // ��ɫ
        default: boxColor = RGB(128, 128, 128);            // ��ɫ
        }

        // ���ƾ����߿�Ч��
        // ��߿�
        setfillcolor(RGB(30, 30, 30));
        solidrectangle(x - 2, y - 2, x + WARNING_BOX_WIDTH + 2, y + WARNING_BOX_HEIGHT + 2);

        // �ڲ���???
        setfillcolor(isActive ? RGB(60, 60, 60) : RGB(40, 40, 40));
        solidrectangle(x, y, x + WARNING_BOX_WIDTH, y + WARNING_BOX_HEIGHT);

        // ���Ʊ߿�߹�
        setcolor(isActive ? boxColor : RGB(80, 80, 80));
        rectangle(x - 1, y - 1, x + WARNING_BOX_WIDTH + 1, y + WARNING_BOX_HEIGHT + 1);

        // ���ƾ����ı�
        settextstyle(16, 0, _T("Arial"));
        settextcolor(isActive ? boxColor : RGB(128, 128, 128));
        wstring wstr(message.begin(), message.end());
        outtextxy(x + 10, y + 5, wstr.c_str());

        // ������漤��������ָʾ��
        if (isActive) {
            setfillcolor(boxColor);
            solidrectangle(x, y, x + 3, y + WARNING_BOX_HEIGHT);
        }
    }

    void drawAllWarningBoxes() {
        int x = WARNING_START_X;
        int y = WARNING_START_Y;
        const vector<WarningMessage>& warnings = engine->getWarnings();

        // �������п��ܵľ���
        struct WarningDef {
            string message;
            WarningLevel level;
        };

        WarningDef allWarnings[] = {
            // �������쳣
            {"Single N1 Sensor Failure", NORMAL},           // ����ת�ٴ���������
            {"Engine N1 Sensor Failure", CAUTION},          // ����ת�ٴ���������
            {"Single EGT Sensor Failure", NORMAL},          // ����EGT����������
            {"Engine EGT Sensor Failure", CAUTION},         // ����EGT����������
            {"Dual Engine Sensor Failure - Shutdown", WARNING},  // ˫������������

            // ȼ���쳣
            {"Low Fuel Level", CAUTION},                    // ȼ��������
            {"Fuel Sensor Failure", WARNING},               // ȼ����������
            {"Fuel Flow Exceeded Limit", CAUTION},          // ȼ�����ٳ���

            // ת���쳣
            {"N1 Overspeed Level 1", CAUTION},             // ��ת1
            {"N1 Overspeed Level 2 - Engine Shutdown", WARNING}, // ��ת2

            // �¶��쳣
            {"EGT Overtemp Level 1 During Start", CAUTION},      // ����1
            {"EGT Overtemp Level 2 During Start - Shutdown", WARNING}, // ����2
            {"EGT Overtemp Level 1 During Run", CAUTION},        // ����3
            {"EGT Overtemp Level 2 During Run - Shutdown", WARNING}    // ����4
        };

        // �������о����
        for (const auto& warningDef : allWarnings) {
            bool isActive = false;

            // ��鵱ǰ�����Ƿ񼤻�
            for (const auto& warning : warnings) {
                // �����Ϣƥ�䲢��ʱ����5����
                if (warning.message == warningDef.message &&
                    warning.timestamp >= engine->getCurrentTime() - 5.0) {
                    isActive = true;
                    break;
                }
            }

            drawWarningBox(warningDef.message, warningDef.level, isActive, x, y);
            y += WARNING_BOX_HEIGHT + WARNING_SPACING;

            // ÿ6�����滻��
            if (y > WARNING_START_Y + 6 * (WARNING_BOX_HEIGHT + WARNING_SPACING)) {
                x += WARNING_BOX_WIDTH + WARNING_SPACING;
                y = WARNING_START_Y;
            }
        }
    }

    void drawFuelFlow() {
        double fuelFlow = engine->getFuelFlow();

        // ����ȼ�����پ�����ɫ
        COLORREF textColor, borderColor, bgColor;
        if (fuelFlow > 0) {
            if (fuelFlow > 50) {
                // ȼ�����ٳ��� - ����ɫ����
                textColor = RGB(255, 128, 0);   // ����ɫ����
                borderColor = RGB(255, 128, 0); // ����ɫ�߿�
                bgColor = RGB(40, 40, 40);      // ����ɫ����
            }
            else {
                // ��������״̬ - ��ɫ
                textColor = WHITE;              // ��ɫ����
                borderColor = RGB(100, 100, 100); // ���ɫ�߿�
                bgColor = RGB(40, 40, 40);       // ����ɫ����
            }
        }
        else {
            // ������״̬ - ��ɫ
            textColor = RGB(150, 150, 150);   // ��ɫ����
            borderColor = RGB(100, 100, 100); // ���ɫ�߿�
            bgColor = RGB(40, 40, 40);       // ����ɫ����
        }

        // �����ı���ʽ
        settextstyle(24, 0, _T("Arial"));
        settextcolor(textColor);

        // ������ʾ�ı�
        TCHAR flowText[64];
        _stprintf_s(flowText, _T("Fuel Flow: %.1f kg/h"), fuelFlow);

        // ������ʾλ��
        int x = 50;
        int y = 560;
        int width = 200;
        int height = 30;

        // ������߿���ӰЧ��
        setfillcolor(RGB(30, 30, 30));
        solidrectangle(x - 1, y - 1, x + width + 1, y + height + 1);

        // ����������
        setfillcolor(bgColor);
        solidrectangle(x, y, x + width, y + height);

        // ���Ʊ߿�
        setcolor(borderColor);
        rectangle(x, y, x + width, y + height);

        // ������ڻ�Ծ״̬���������ָʾ��
        if (fuelFlow > 0) {
            setfillcolor(borderColor);
            solidrectangle(x, y, x + 3, y + height);
        }

        // �����ı�����΢����ƫ���Աܿ����ָʾ����
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
        drawFuelFlow();  // ȷ��������Ԫ��֮�����

        SetWorkingImage(NULL);
        putimage(0, 0, backBuffer);

        lastFrameTime = currentTime;
    }

    bool checkButtonClick(const Button& btn, int mouseX, int mouseY) {
        return mouseX >= btn.x && mouseX <= btn.x + btn.width &&
            mouseY >= btn.y && mouseY <= btn.y + btn.height;
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
        double simulationTime = 0.0;  // ģ��ʱ��

        bool running = true;

        // Ԥ��ѭ��
        for (int i = 0; i < 100; i++) {
            engine.update(fixedTimeStep);
        }

        while (running) {
            currentTime = GetTickCount64();
            double deltaTime = (currentTime - lastUpdateTime) / 1000.0;
            if (deltaTime > 0.25) deltaTime = 0.25;
            lastUpdateTime = currentTime;

            accumulator += deltaTime;

            // ��������
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

            // ��������
            while (accumulator >= fixedTimeStep) {
                engine.update(fixedTimeStep);
                engine.checkWarnings(currentTime / 1000.0);

                // ��¼���� - ʹ�þ�ȷ��ģ��ʱ��
                logger.logData(
                    simulationTime,
                    engine.getN1(),
                    engine.getTemperature(),
                    engine.getFuelFlow(),
                    engine.getFuelAmount()
                );

                accumulator -= fixedTimeStep;
                simulationTime += fixedTimeStep;  // ֻ���������ģ��ʱ��
            }

            gui.update();

            if (GetAsyncKeyState(VK_ESCAPE)) {
                running = false;
            }

            // ��ȷ����ѭ��ʱ��
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