#pragma once

class Sensor {
protected:
    double value;
    bool isValid;

public:
    // ���캯��
    Sensor();

    // ��ȡ������ֵ
    virtual double getValue();

    // ���ô�����ֵ
    virtual void setValue(double v);

    // ��ȡ��������Ч��
    bool getValidity();

    // ���ô�������Ч��
    void setValidity(bool valid);
};