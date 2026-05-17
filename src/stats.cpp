#include "stats.h"
#include <iostream>
#include <iomanip>

void StatsManager::update(const ProcessResult& result) {
    m_total++;
    m_totalTimeMs += result.processTimeMs;

    switch (result.idStatus) {
    case IdStatus::QR_SUCCESS:       m_qrSuccess++; break;
    case IdStatus::OCR_RECOVERED:    m_ocrRecovered++; break;
    case IdStatus::OCR_CORRECTED:    m_ocrCorrected++; break;
    case IdStatus::LABEL_ERROR:      m_labelError++; break;
    case IdStatus::UNKNOWN_PACKAGE:  m_unknownPackage++; break;
    }

    if (result.sortStatus == SortStatus::WRONG_SORT) {
        m_wrongSort++;
    }

    if (result.riskLevel == RiskLevel::LEVEL_3_HIGH ||
        result.riskLevel == RiskLevel::LEVEL_4_CRITICAL) {
        m_level3Plus4++;
    }

    // V3 quality and decision tracking
    if (result.action == "PASS")           m_passCount++;
    else if (result.action == "PASS_WITH_LOG") m_passWithLogCount++;
    else if (result.action == "REVIEW")    m_reviewCount++;
    else if (result.action == "BLOCK")     m_blockCount++;

    m_totalConfidence += result.confidence;

    if (result.qualityLevel == "GOOD")       m_qualityGood++;
    else if (result.qualityLevel == "WARNING") m_qualityWarning++;
    else if (result.qualityLevel == "BAD")    m_qualityBad++;
}

void StatsManager::addRecentEvent(const std::string& msg) {
    m_recentEvents.push_back(msg);
    if (m_recentEvents.size() > 5) {
        m_recentEvents.erase(m_recentEvents.begin());
    }
}

void StatsManager::printSummary() const {
    std::cout << "\n";
    std::cout << "========== 系统处理统计 ==========\n";
    std::cout << "总处理数量：" << m_total << "\n";
    std::cout << "QR识别成功：" << m_qrSuccess << "\n";
    std::cout << "OCR补救成功：" << m_ocrRecovered << "\n";
    std::cout << "OCR纠正成功：" << m_ocrCorrected << "\n";
    std::cout << "标签异常：" << m_labelError << "\n";
    std::cout << "未知货物：" << m_unknownPackage << "\n";
    std::cout << "错误异常：" << m_wrongSort << "\n";

    int totalRecognized = m_qrSuccess + m_ocrRecovered + m_ocrCorrected;
    int ocrAttempts = m_total - m_qrSuccess;
    int ocrTotalSuccess = m_ocrRecovered + m_ocrCorrected;

    double overallRate = (m_total > 0) ? (100.0 * totalRecognized / m_total) : 0;
    double ocrRecoveryRate = (ocrAttempts > 0) ? (100.0 * ocrTotalSuccess / ocrAttempts) : 0;
    double ocrCorrectRate = (ocrTotalSuccess > 0) ? (100.0 * m_ocrCorrected / ocrTotalSuccess) : 0;
    double avgTime = (m_total > 0) ? (static_cast<double>(m_totalTimeMs) / m_total) : 0;

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "综合识别成功率：" << overallRate << "%\n";
    std::cout << "OCR恢复率：" << ocrRecoveryRate << "%\n";
    std::cout << "OCR纠正率：" << ocrCorrectRate << "%\n";
    std::cout << "高风险事件数：" << m_level3Plus4 << "\n";
    std::cout << "平均处理时间：" << static_cast<int>(avgTime) << " ms\n";
    std::cout << "---------------------------------\n";
    std::cout << "--- V3 QualityGate 统计 ---\n";
    std::cout << "标签质量-GOOD：" << m_qualityGood << "\n";
    std::cout << "标签质量-WARNING：" << m_qualityWarning << "\n";
    std::cout << "标签质量-BAD：" << m_qualityBad << "\n";
    std::cout << "平均置信度：" << avgConfidence() << "\n";
    std::cout << "决策-PASS：" << m_passCount << "\n";
    std::cout << "决策-PASS_WITH_LOG：" << m_passWithLogCount << "\n";
    std::cout << "决策-REVIEW：" << m_reviewCount << "\n";
    std::cout << "决策-BLOCK：" << m_blockCount << "\n";
    std::cout << "=================================\n";
}
