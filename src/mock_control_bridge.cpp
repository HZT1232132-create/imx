#include "control_bridge.h"
#include "mock_control_bridge.h"
#include <iostream>

class MockControlBridge : public IControlBridge {
public:
    ControlStatus sendCommand(const ControlCommand& cmd) override {
        ControlStatus st;
        st.heartbeatCount = ++m_heartbeat;
        st.commandRxOk   = true;
        st.sortControlOk = true;
        st.safetyTaskOk  = true;
        st.statusTxOk    = true;
        st.virtualIOOk   = true;

        // Map risk level → M33 state
        if (cmd.emergencyStop) {
            st.state = "SAFETY_LOCK";
            st.led = "red"; st.buzzer = "continuous"; st.motor = "stop";
            st.chute = "block"; st.gate = "closed";
            st.safetyLocked = true;
            st.reason = "EMERGENCY_STOP";
        } else if (cmd.action == "BLOCK") {
            st.state = "SORT_BLOCK";
            st.led = "red"; st.buzzer = "continuous"; st.motor = "stop";
            st.chute = "block"; st.gate = "closed";
            st.reason = "WRONG_SORT_LEVEL_4";
        } else if (cmd.action == "REVIEW") {
            st.state = "SORT_REVIEW";
            st.led = "yellow"; st.buzzer = "slow"; st.motor = "slow";
            st.chute = "review"; st.gate = "divert";
            st.reason = "LOW_CONFIDENCE";
        } else if (cmd.riskLevel >= 3) {
            st.state = "SORT_BLOCK";
            st.led = "red"; st.buzzer = "fast"; st.motor = "stop";
            st.chute = "block"; st.gate = "closed";
            st.reason = "HIGH_RISK_LEVEL_" + std::to_string(cmd.riskLevel);
        } else if (cmd.riskLevel == 2) {
            st.state = "SORT_REVIEW";
            st.led = "yellow"; st.buzzer = "slow"; st.motor = "slow";
            st.chute = "review"; st.gate = "divert";
            st.reason = "MEDIUM_RISK_REVIEW";
        } else {
            st.state = "SORT_ROUTE_" + cmd.targetZone;
            st.led = "green"; st.buzzer = "off"; st.motor = "run";
            st.chute = cmd.targetZone; st.gate = "open";
            st.reason = "PASS_TO_TARGET_ZONE";
        }

        std::cout << "[M33-Mock] " << st.state << " led=" << st.led
                  << " motor=" << st.motor << " gate=" << st.gate << std::endl;
        return st;
    }

    ControlStatus getStatus() override { return m_lastStatus; }
    void reset() override { m_lastStatus = ControlStatus{}; m_heartbeat = 0; }
    bool isConnected() const override { return true; }

private:
    ControlStatus m_lastStatus;
    int m_heartbeat = 0;
    std::string reason;
};

IControlBridge* createMockControlBridge() { return new MockControlBridge(); }
