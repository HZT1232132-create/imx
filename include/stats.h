#pragma once

#include <vector>
#include <string>
#include "process_result.h"

class StatsManager {
public:
    void update(const ProcessResult& result);
    void printSummary() const;

    // Getters for HUD
    int total() const { return m_total; }
    int qrSuccess() const { return m_qrSuccess; }
    int ocrRecovered() const { return m_ocrRecovered; }
    int ocrCorrected() const { return m_ocrCorrected; }
    int labelError() const { return m_labelError; }
    int unknownPackage() const { return m_unknownPackage; }
    int wrongSort() const { return m_wrongSort; }
    int highRiskEvents() const { return m_level3Plus4; }
    int avgTimeMs() const { return m_total > 0 ? static_cast<int>(m_totalTimeMs / m_total) : 0; }
    int recognizedCount() const { return m_qrSuccess + m_ocrRecovered + m_ocrCorrected; }
    double overallRate() const { return m_total > 0 ? 100.0 * recognizedCount() / m_total : 0.0; }

    // V3 getters
    int passCount() const { return m_passCount; }
    int passWithLogCount() const { return m_passWithLogCount; }
    int reviewCount() const { return m_reviewCount; }
    int blockCount() const { return m_blockCount; }
    double avgConfidence() const { return m_total > 0 ? m_totalConfidence / m_total : 0.0; }
    int qualityGood() const { return m_qualityGood; }
    int qualityWarning() const { return m_qualityWarning; }
    int qualityBad() const { return m_qualityBad; }

    // Recent events for HUD
    void addRecentEvent(const std::string& msg);
    const std::vector<std::string>& recentEvents() const { return m_recentEvents; }

private:
    int m_total = 0;
    int m_qrSuccess = 0;
    int m_ocrRecovered = 0;
    int m_ocrCorrected = 0;
    int m_labelError = 0;
    int m_unknownPackage = 0;
    int m_wrongSort = 0;
    int m_level3Plus4 = 0;
    long long m_totalTimeMs = 0;

    // V3 counters
    int m_passCount = 0;
    int m_passWithLogCount = 0;
    int m_reviewCount = 0;
    int m_blockCount = 0;
    double m_totalConfidence = 0.0;
    int m_qualityGood = 0;
    int m_qualityWarning = 0;
    int m_qualityBad = 0;

    std::vector<std::string> m_recentEvents;
};
