#include "alarm_simulator.h"
#include <iostream>

void AlarmSimulator::apply(RiskLevel risk) {
    m_currentLevel = risk;
    switch (risk) {
    case RiskLevel::LEVEL_0_NORMAL:
        green(true); yellow(false); red(false); buzzer(false);
        break;
    case RiskLevel::LEVEL_1_LOW:
        green(true); yellow(true); red(false); buzzer(false); // green + yellow flash
        break;
    case RiskLevel::LEVEL_2_MEDIUM:
        green(false); yellow(true); red(false); buzzer(false); // yellow only
        break;
    case RiskLevel::LEVEL_3_HIGH:
        green(false); yellow(false); red(true); buzzer(false); // red only, no buzzer
        break;
    case RiskLevel::LEVEL_4_CRITICAL:
        green(false); yellow(false); red(true); buzzer(true);  // red + buzzer
        break;
    }
}

void AlarmSimulator::reset() {
    green(false); yellow(false); red(false); buzzer(false);
    m_currentLevel = RiskLevel::LEVEL_0_NORMAL;
}

void AlarmSimulator::green(bool on) {
    std::cout << "[GPIO] GREEN=" << (on ? "ON " : "OFF") << "  ";
}

void AlarmSimulator::yellow(bool on) {
    std::cout << "YELLOW=" << (on ? "ON " : "OFF") << "  ";
}

void AlarmSimulator::red(bool on) {
    std::cout << "RED=" << (on ? "ON " : "OFF") << "  ";
}

void AlarmSimulator::buzzer(bool on) {
    std::cout << "BUZZER=" << (on ? "ON " : "OFF");
    std::cout << std::endl;
}
