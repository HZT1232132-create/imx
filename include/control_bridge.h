#pragma once

#include <string>

// M33 控制状态 — A55 → M33 命令 和 M33 → A55 反馈
struct ControlCommand {
    // A55 → M33
    int frameId = 0;
    std::string packageId;
    std::string targetZone;
    std::string currentZone;
    int riskLevel     = 0;    // 0-4
    std::string action;       // PASS / PASS_WITH_LOG / REVIEW / BLOCK
    std::string decisionReason;
    bool emergencyStop = false;
    int timeoutMs = 5000;
};

struct ControlStatus {
    // M33 → A55
    std::string rtosVersion = "FreeRTOS";
    std::string state;          // "IDLE" / "SORT_ROUTE_A" / "SORT_ROUTE_B" / "SORT_ROUTE_C"
                                // "SORT_REVIEW" / "SORT_BLOCK" / "SAFETY_LOCK"

    // 执行器状态
    std::string led;            // "green" / "yellow" / "red" / "off"
    std::string buzzer;         // "off" / "slow" / "fast" / "continuous"
    std::string motor;          // "run" / "slow" / "stop"
    std::string chute;          // "A" / "B" / "C" / "review" / "block"
    std::string gate;           // "open" / "divert" / "closed"

    // 安全
    bool heartbeatOk  = true;
    bool safetyLocked = false;
    int heartbeatCount = 0;
    int timeoutCount   = 0;

    // 任务状态
    bool commandRxOk   = true;
    bool sortControlOk = true;
    bool safetyTaskOk  = true;
    bool statusTxOk    = true;
    bool virtualIOOk   = true;
};

// A55 ↔ M33 通信抽象接口
class IControlBridge {
public:
    virtual ~IControlBridge() = default;

    // 发送命令给 M33, 返回当前控制状态
    virtual ControlStatus sendCommand(const ControlCommand& cmd) = 0;

    // 查询 M33 最新状态
    virtual ControlStatus getStatus() = 0;

    // 复位
    virtual void reset() = 0;

    virtual bool isConnected() const = 0;
};
