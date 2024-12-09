#pragma once

class Sensor {
protected:
    double value;
    bool isValid;

public:
    // 构造函数
    Sensor();

    // 获取传感器值
    virtual double getValue();

    // 设置传感器值
    virtual void setValue(double v);

    // 获取传感器有效性
    bool getValidity();

    // 设置传感器有效性
    void setValidity(bool valid);
};