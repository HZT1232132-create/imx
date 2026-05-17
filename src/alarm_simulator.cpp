#include "alarm_simulator.h"
#include <iostream>

AlarmSimulator::AlarmSimulator() {
    m_chip = gpiod_chip_open("/dev/gpiochip2");
    if (!m_chip) {
        std::cerr << "[GPIO] Cannot open gpiochip2 (run as root?)\n";
        return;
    }
    // offsets: 0=green, 1=yellow, 2=red, 3=buzzer
    m_green  = gpiod_chip_get_line(m_chip, 0);
    m_yellow = gpiod_chip_get_line(m_chip, 1);
    m_red    = gpiod_chip_get_line(m_chip, 2);
    m_buzzer = gpiod_chip_get_line(m_chip, 3);

    // Request all as outputs, initial LOW
    if (m_green)  gpiod_line_request_output(m_green,  "green",  0);
    if (m_yellow) gpiod_line_request_output(m_yellow, "yellow", 0);
    if (m_red)    gpiod_line_request_output(m_red,    "red",    0);
    if (m_buzzer) gpiod_line_request_output(m_buzzer, "buzzer", 0);

    std::cout << "[GPIO] Initialized gpiochip2 (green=0, yellow=1, red=2, buzzer=3)\n";
}

AlarmSimulator::~AlarmSimulator() {
    if (m_chip) gpiod_chip_close(m_chip);
}

void AlarmSimulator::apply(RiskLevel risk) {
    m_currentLevel = risk;
    switch (risk) {
    case RiskLevel::LEVEL_0_NORMAL:
        green(true); yellow(false); red(false); buzzer(false);
        break;
    case RiskLevel::LEVEL_1_LOW:
        green(true); yellow(true); red(false); buzzer(false);
        break;
    case RiskLevel::LEVEL_2_MEDIUM:
        green(false); yellow(true); red(false); buzzer(false);
        break;
    case RiskLevel::LEVEL_3_HIGH:
        green(false); yellow(false); red(true); buzzer(false);
        break;
    case RiskLevel::LEVEL_4_CRITICAL:
        green(false); yellow(false); red(true); buzzer(true);
        break;
    }
}

void AlarmSimulator::reset() {
    green(false); yellow(false); red(false); buzzer(false);
    m_currentLevel = RiskLevel::LEVEL_0_NORMAL;
}

void AlarmSimulator::green(bool on) {
    if (m_green) gpiod_line_set_value(m_green, on ? 1 : 0);
    std::cout << "[GPIO] GREEN=" << (on ? "ON " : "OFF") << "  ";
}

void AlarmSimulator::yellow(bool on) {
    if (m_yellow) gpiod_line_set_value(m_yellow, on ? 1 : 0);
    std::cout << "YELLOW=" << (on ? "ON " : "OFF") << "  ";
}

void AlarmSimulator::red(bool on) {
    if (m_red) gpiod_line_set_value(m_red, on ? 1 : 0);
    std::cout << "RED=" << (on ? "ON " : "OFF") << "  ";
}

void AlarmSimulator::buzzer(bool on) {
    if (m_buzzer) gpiod_line_set_value(m_buzzer, on ? 1 : 0);
    std::cout << "BUZZER=" << (on ? "ON " : "OFF");
    std::cout << std::endl;
}
