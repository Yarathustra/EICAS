#include "Sensor.h"

Sensor::Sensor() : value(0), isValid(true) {
}

double Sensor::getValue() {
    // 当传感器失效时返回0
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
    // 如果传感器变为无效，立即将值设为0
    if (!valid) {
        value = 0.0;
    }
}