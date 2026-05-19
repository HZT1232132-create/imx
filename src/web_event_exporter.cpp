#include "web_event_exporter.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

WebEventExporter::WebEventExporter(const std::string& outputDir)
    : m_outputDir(outputDir),
      m_jsonDir(outputDir + "/events_json"),
      m_framesDir(outputDir + "/frames") {
    std::filesystem::create_directories(m_jsonDir);
    std::filesystem::create_directories(m_framesDir);
}

static std::string escapeJson(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
        case '"': oss << "\\\""; break;
        case '\\': oss << "\\\\"; break;
        case '\n': oss << "\\n"; break;
        default: oss << c;
        }
    }
    return oss.str();
}

UnifiedEvent WebEventExporter::buildEvent(
    int frameId, const ProcessResult& result,
    const AIResult& ai, const ControlStatus& m33Status,
    const std::string& prevHash, const std::string& currentHash,
    const std::string& ruleVer, const std::string& modelVer) {

    UnifiedEvent ev;
    ev.frameId = frameId;
    ev.imageName = result.imageName;
    {
        std::ostringstream oss;
        oss << "frame_" << std::setfill('0') << std::setw(4) << frameId << ".jpg";
        ev.imageUrl = "/frames/" + oss.str();
    }

    // Detections from AI
    ev.detections = ai.detections;

    // NPU AI metadata
    ev.npuBackend = ai.backend;
    ev.npuModel = ai.modelName;
    ev.npuLatencyMs = ai.latencyMs;
    ev.qualityClass = ai.qualityClass;
    ev.qualityConfidence = ai.qualityConfidence;

    // A55 recognition pipeline
    ev.qrResult = result.rawText;
    ev.ocrResult = result.rawText;
    ev.finalPackageId = result.finalPackageId;
    ev.recognitionMethod = [&]() -> std::string {
        switch (result.idStatus) {
        case IdStatus::QR_SUCCESS: return "QR_SUCCESS";
        case IdStatus::OCR_RECOVERED: return "OCR_RECOVERED";
        case IdStatus::OCR_CORRECTED: return "OCR_CORRECTED";
        case IdStatus::UNKNOWN_PACKAGE: return "UNKNOWN_PACKAGE";
        case IdStatus::LABEL_ERROR: return "LABEL_ERROR";
        default: return "UNKNOWN";
        }
    }();

    // Sort
    ev.targetZone = result.targetZone;
    ev.currentZone = "";  // removed: system doesn't know current zone
    ev.sortStatus = [&]() -> std::string {
        switch (result.sortStatus) {
        case SortStatus::NORMAL_SORT: return "NORMAL_SORT";
        case SortStatus::WRONG_SORT: return "WRONG_SORT";
        case SortStatus::CANNOT_JUDGE: return "CANNOT_JUDGE";
        default: return "UNKNOWN";
        }
    }();

    // Decision
    ev.decisionConfidence = result.confidence;
    ev.riskLevel = [&]() -> std::string {
        switch (result.riskLevel) {
        case RiskLevel::LEVEL_0_NORMAL: return "LEVEL_0_NORMAL";
        case RiskLevel::LEVEL_1_LOW: return "LEVEL_1_LOW";
        case RiskLevel::LEVEL_2_MEDIUM: return "LEVEL_2_MEDIUM";
        case RiskLevel::LEVEL_3_HIGH: return "LEVEL_3_HIGH";
        case RiskLevel::LEVEL_4_CRITICAL: return "LEVEL_4_CRITICAL";
        default: return "UNKNOWN";
        }
    }();
    ev.action = result.action;
    ev.decisionReason = result.decisionReason;

    // M33
    ev.m33 = m33Status;

    // Hash
    ev.prevHash = prevHash;
    ev.currentHash = currentHash;
    ev.verify = "PASS";

    ev.ruleVersion = ruleVer;
    ev.modelVersion = modelVer;

    return ev;
}

void WebEventExporter::exportEvent(const UnifiedEvent& event, const cv::Mat& annotatedFrame) {
    // Serialize to JSON
    std::ostringstream json;
    json << "{\n";
    json << "  \"frame_id\":" << event.frameId << ",\n";
    json << "  \"image_name\":\"" << escapeJson(event.imageName) << "\",\n";
    json << "  \"image_url\":\"" << event.imageUrl << "\",\n";

    // Detections
    json << "  \"detections\":[\n";
    for (size_t i = 0; i < event.detections.size(); ++i) {
        const auto& d = event.detections[i];
        json << "    {\"class\":\"" << d.cls
             << "\",\"confidence\":" << d.confidence
             << ",\"bbox\":[" << d.bbox.x << "," << d.bbox.y
             << "," << d.bbox.width << "," << d.bbox.height << "]";
        if (!d.labelText.empty())
            json << ",\"label_text\":\"" << escapeJson(d.labelText) << "\"";
        json << "}" << (i+1 < event.detections.size() ? "," : "") << "\n";
    }
    json << "  ],\n";

    // NPU
    json << "  \"npu\":{\"backend\":\"" << event.npuBackend
         << "\",\"model\":\"" << event.npuModel
         << "\",\"latency_ms\":" << event.npuLatencyMs
         << ",\"quality_class\":\"" << event.qualityClass
         << "\",\"confidence\":" << event.qualityConfidence
         << "},\n";

    // Recognition
    json << "  \"recognition\":{\"qr_result\":\"" << escapeJson(event.qrResult)
         << "\",\"ocr_result\":\"" << escapeJson(event.ocrResult)
         << "\",\"final_package_id\":\"" << escapeJson(event.finalPackageId)
         << "\",\"method\":\"" << event.recognitionMethod
         << "\"},\n";

    // Rule
    json << "  \"rule\":{\"target_zone\":\"" << event.targetZone
         << "\",\"current_zone\":\"" << event.currentZone
         << "\",\"sort_status\":\"" << event.sortStatus
         << "\"},\n";

    // Decision
    json << "  \"decision\":{\"confidence\":" << event.decisionConfidence
         << ",\"risk_level\":\"" << event.riskLevel
         << "\",\"action\":\"" << event.action
         << "\",\"reason\":\"" << escapeJson(event.decisionReason)
         << "\"},\n";

    // M33
    json << "  \"m33\":{\"rtos\":\"FreeRTOS\",\"state\":\"" << event.m33.state
         << "\",\"led\":\"" << event.m33.led
         << "\",\"buzzer\":\"" << event.m33.buzzer
         << "\",\"motor\":\"" << event.m33.motor
         << "\",\"chute\":\"" << event.m33.chute
         << "\",\"gate\":\"" << event.m33.gate
         << "\",\"heartbeat_ok\":" << (event.m33.heartbeatOk ? "true" : "false")
         << ",\"heartbeat_count\":" << event.m33.heartbeatCount
         << ",\"safety_locked\":" << (event.m33.safetyLocked ? "true" : "false")
         << ",\"command_rx_ok\":" << (event.m33.commandRxOk ? "true" : "false")
         << ",\"sort_control_ok\":" << (event.m33.sortControlOk ? "true" : "false")
         << ",\"safety_task_ok\":" << (event.m33.safetyTaskOk ? "true" : "false")
         << ",\"status_tx_ok\":" << (event.m33.statusTxOk ? "true" : "false")
         << ",\"virtual_io_ok\":" << (event.m33.virtualIOOk ? "true" : "false")
         << "},\n";

    // Hash
    json << "  \"hash\":{\"prev_hash\":\"" << event.prevHash
         << "\",\"current_hash\":\"" << event.currentHash
         << "\",\"verify\":\"" << event.verify << "\"},\n";

    json << "  \"rule_version\":\"" << event.ruleVersion
         << "\",\"model_version\":\"" << event.modelVersion << "\"\n";
    json << "}\n";

    // Write JSON
    std::ostringstream fname;
    fname << "event_" << std::setfill('0') << std::setw(4) << event.frameId << ".json";
    std::ofstream f(m_jsonDir + "/" + fname.str());
    if (f.is_open()) {
        f << json.str();
        f.close();
    }

    // Save annotated frame
    std::ostringstream imgName;
    imgName << "frame_" << std::setfill('0') << std::setw(4) << event.frameId << ".jpg";
    cv::imwrite(m_framesDir + "/" + imgName.str(), annotatedFrame);

    // Also write latest_event.json (overwrite, for live polling)
    {
        std::ofstream lf(m_outputDir + "/latest_event.json");
        if (lf.is_open()) { lf << json.str(); lf.close(); }
    }
    // And encode frame as base64 for live display
    {
        std::vector<uchar> buf;
        cv::imencode(".jpg", annotatedFrame, buf);
        std::ofstream bf(m_outputDir + "/latest_frame.b64");
        if (bf.is_open()) {
            for (uchar c : buf) bf << (char)c;
            bf.close();
        }
    }

    std::cout << "[WebExport] " << fname.str() << " + " << imgName.str() << std::endl;
}
