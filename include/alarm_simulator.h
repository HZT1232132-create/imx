#pragma once

#include "risk_engine.h"

class AlarmSimulator {
public:
    void apply(RiskLevel risk);
    void reset();

private:
    RiskLevel m_currentLevel = RiskLevel::LEVEL_0_NORMAL;

    void green(bool on);
    void yellow(bool on);
    void red(bool on);
    void buzzer(bool on);
};
