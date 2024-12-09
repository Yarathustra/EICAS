#include "Sensor.h"

Sensor::Sensor() : value(0), isValid(true) {
}

double Sensor::getValue() {
    // ��������ʧЧʱ����0
    return isValid ? value : 0.0;
}

void Sensor::setValue(double v) {
    value = v;
}

bool Sensor::getValidity() {
    return isValid;
}

void Sensor::setValidity(bool valid) {
    isValid = valid;
    // �����������Ϊ��Ч��������ֵ��Ϊ0
    if (!valid) {
        value = 0.0;
    }
}