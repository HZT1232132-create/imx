#pragma once

#include "risk_engine.h"
#include <string>
#include <gpiod.h>

class AlarmSimulator {
public:
    AlarmSimulator();
    ~AlarmSimulator();
    void apply(RiskLevel risk);
    void reset();

private:
    struct gpiod_chip* m_chip = nullptr;
    struct gpiod_line* m_green  = nullptr;
    struct gpiod_line* m_yellow = nullptr;
    struct gpiod_line* m_red    = nullptr;
    struct gpiod_line* m_buzzer = nullptr;
    RiskLevel m_currentLevel = RiskLevel::LEVEL_0_NORMAL;

    void green(bool on);
    void yellow(bool on);
    void red(bool on);
    void buzzer(bool on);
};
