#pragma once

// 警告等级枚举
enum WarningLevel {
    NORMAL,     // 正常
    CAUTION,    // 注意
    WARNING,    // 警告
    INVALID     // 无效
};

// 警告消息结构
struct WarningMessage {
    std::string message;
    WarningLevel level;
    double timestamp;

    WarningMessage(const std::string& msg, WarningLevel lvl, double time)
        : message(msg), level(lvl), timestamp(time) {}
}; 