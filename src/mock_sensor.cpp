#include "mock_sensor.h"
#include <fstream>
#include <sstream>
#include <iostream>

MockSensor::MockSensor() = default;

bool MockSensor::loadScript(const std::string& csvPath) {
    std::ifstream f(csvPath);
    if (!f.is_open()) {
        std::cerr << "[MockSensor] Cannot open: " << csvPath << "\n";
        return false;
    }

    m_script.clear();
    std::string line;
    // Skip header
    std::getline(f, line);
    while (std::getline(f, line)) {
        SensorState st;
        std::istringstream iss(line);
        std::string token;
        int col = 0;
        while (std::getline(iss, token, ',')) {
            int val = std::stoi(token);
            switch (col) {
            case 0: st.itemPresent  = val != 0; break;
            case 1: st.gateOpen    = val != 0; break;
            case 2: st.gateClosed  = val != 0; break;
            case 3: st.motorRunning = val != 0; break;
            case 4: st.emergencyStop = val != 0; break;
            }
            col++;
        }
        m_script.push_back(st);
    }
    std::cout << "[MockSensor] Loaded " << m_script.size() << " sensor states\n";
    return !m_script.empty();
}

SensorState MockSensor::getState(int frameId) const {
    if (m_manualEmergency) {
        SensorState st;
        st.emergencyStop = true;
        return st;
    }
    if (frameId < 1 || frameId > static_cast<int>(m_script.size()))
        return SensorState{true, false, true, true, false};  // default: item present, gate closed, motor running
    return m_script[frameId - 1];
}

void MockSensor::triggerEmergencyStop() {
    m_manualEmergency = true;
    std::cout << "[MockSensor] EMERGENCY STOP triggered!\n";
}

void MockSensor::clearEmergencyStop() {
    m_manualEmergency = false;
    std::cout << "[MockSensor] Emergency stop cleared.\n";
}
