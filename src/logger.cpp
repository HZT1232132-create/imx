#include "logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

static std::string csvEscape(const std::string& s) {
    if (s.find(',') == std::string::npos && s.find('"') == std::string::npos
        && s.find('\n') == std::string::npos && s.find('\r') == std::string::npos) {
        return s;
    }
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

EventLogger::EventLogger(const std::string& logPath) {
    m_file.open(logPath, std::ios::out);
    if (m_file.is_open()) {
        writeHeader();
    }
}

EventLogger::~EventLogger() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

bool EventLogger::isOpen() const {
    return m_file.is_open();
}

void EventLogger::writeHeader() {
    m_file << "time,image,scene,raw_text,final_package_id,recognition_method,"
           << "id_status,target_zone,current_zone,sort_status,risk_level,"
           << "process_time_ms,quality_level,quality_score,confidence,action,reason,message\n";
}

void EventLogger::write(const ProcessResult& result) {
    if (!m_file.is_open()) return;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);

    const char* method = result.recognitionMethod.empty() ? "NONE" : result.recognitionMethod.c_str();

    m_file << timeBuf << ","
           << csvEscape(result.imageName) << ","
           << csvEscape(result.sceneDesc) << ","
           << csvEscape(result.rawText) << ","
           << csvEscape(result.finalPackageId) << ","
           << method << ","
           << RiskEngine().idStatusName(result.idStatus) << ","
           << result.targetZone << ","
           << result.currentZone << ","
           << RiskEngine().sortStatusName(result.sortStatus) << ","
           << RiskEngine().levelName(result.riskLevel) << ","
           << result.processTimeMs << ","
           << result.qualityLevel << ","
           << result.qualityScore << ","
           << result.confidence << ","
           << result.action << ","
           << csvEscape(result.decisionReason) << ","
           << csvEscape(result.message) << "\n";

    m_file.flush();
}
