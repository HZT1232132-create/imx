#pragma once

#include <string>

// 光电/急停传感器状态
struct SensorState {
    bool itemPresent  = false;   // 货物到达光电传感器
    bool gateOpen      = false;   // 闸门开到位
    bool gateClosed    = false;   // 闸门关到位
    bool motorRunning  = false;   // 电机运转反馈
    bool emergencyStop = false;   // 急停按钮按下
};

class MockSensor {
public:
    MockSensor();

    // 从 CSV 加载传感器序列 (sensor_script.csv)
    bool loadScript(const std::string& csvPath);

    // 获取当前帧传感器状态
    SensorState getState(int frameId) const;

    // 手动触发急停
    void triggerEmergencyStop();

    // 清除急停
    void clearEmergencyStop();

private:
    std::vector<SensorState> m_script;
    bool m_manualEmergency = false;
};
