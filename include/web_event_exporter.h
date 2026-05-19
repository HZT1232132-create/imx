#pragma once

#include <string>
#include <vector>
#include <fstream>
#include "ai_engine.h"
#include "control_bridge.h"
#include "process_result.h"

// 统一事件 JSON — A55、NPU、M33、Web 共享接口
struct UnifiedEvent {
    int frameId = 0;
    std::string imageName;
    std::string imageUrl;       // "/frames/frame_0004.jpg"

    // NPU AI 检测
    std::vector<Detection> detections;

    // NPU AI 元信息
    std::string npuBackend;     // "NPU" / "CPU" / "MOCK"
    std::string npuModel;
    double npuLatencyMs = 0.0;
    std::string qualityClass;
    double qualityConfidence = 0.0;

    // A55 识别管道
    std::string qrResult;
    std::string ocrResult;
    std::string finalPackageId;
    std::string recognitionMethod;  // "QR_SUCCESS" / "OCR_RECOVERED" / "OCR_CORRECTED" / ...

    // 规则 & 分拣
    std::string targetZone;
    std::string currentZone;
    std::string sortStatus;     // "NORMAL_SORT" / "WRONG_SORT" / "CANNOT_JUDGE"

    // 决策
    double decisionConfidence = 0.0;
    std::string riskLevel;      // "LEVEL_0_NORMAL" ~ "LEVEL_4_CRITICAL"
    std::string action;         // "PASS" / "PASS_WITH_LOG" / "REVIEW" / "BLOCK"
    std::string decisionReason;

    // M33 控制状态
    ControlStatus m33;

    // Hash 溯源
    std::string prevHash;
    std::string currentHash;
    std::string verify;         // "PASS" / "FAIL"

    // 版本
    std::string ruleVersion;
    std::string modelVersion;
};

class WebEventExporter {
public:
    WebEventExporter(const std::string& outputDir = "../output");

    // 从流水线结果构建统一事件
    UnifiedEvent buildEvent(int frameId, const ProcessResult& result,
                            const AIResult& ai, const ControlStatus& m33Status,
                            const std::string& prevHash, const std::string& currentHash,
                            const std::string& ruleVer, const std::string& modelVer);

    // 写 event_XXXX.json 和标注帧
    void exportEvent(const UnifiedEvent& event, const cv::Mat& annotatedFrame);

private:
    std::string m_outputDir;
    std::string m_jsonDir;
    std::string m_framesDir;
};
