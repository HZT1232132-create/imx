#include "risk_engine.h"

RiskLevel RiskEngine::map(IdStatus idStatus, SortStatus sortStatus) const {
    // WRONG_SORT always critical regardless of idStatus
    if (sortStatus == SortStatus::WRONG_SORT) {
        return RiskLevel::LEVEL_4_CRITICAL;
    }

    // LABEL_ERROR or UNKNOWN_PACKAGE → HIGH
    if (idStatus == IdStatus::LABEL_ERROR || idStatus == IdStatus::UNKNOWN_PACKAGE) {
        return RiskLevel::LEVEL_3_HIGH;
    }

    // OCR_CORRECTED → MEDIUM (needs human review)
    if (idStatus == IdStatus::OCR_CORRECTED) {
        return RiskLevel::LEVEL_2_MEDIUM;
    }

    // OCR_RECOVERED → LOW
    if (idStatus == IdStatus::OCR_RECOVERED) {
        return RiskLevel::LEVEL_1_LOW;
    }

    // QR_SUCCESS + NORMAL_SORT → NORMAL
    return RiskLevel::LEVEL_0_NORMAL;
}

const char* RiskEngine::levelName(RiskLevel level) const {
    switch (level) {
    case RiskLevel::LEVEL_0_NORMAL:   return "LEVEL_0_NORMAL";
    case RiskLevel::LEVEL_1_LOW:      return "LEVEL_1_LOW";
    case RiskLevel::LEVEL_2_MEDIUM:    return "LEVEL_2_MEDIUM";
    case RiskLevel::LEVEL_3_HIGH:     return "LEVEL_3_HIGH";
    case RiskLevel::LEVEL_4_CRITICAL: return "LEVEL_4_CRITICAL";
    }
    return "UNKNOWN";
}

const char* RiskEngine::idStatusName(IdStatus status) const {
    switch (status) {
    case IdStatus::QR_SUCCESS:       return "QR_SUCCESS";
    case IdStatus::OCR_RECOVERED:    return "OCR_RECOVERED";
    case IdStatus::OCR_CORRECTED:    return "OCR_CORRECTED";
    case IdStatus::UNKNOWN_PACKAGE:  return "UNKNOWN_PACKAGE";
    case IdStatus::LABEL_ERROR:      return "LABEL_ERROR";
    }
    return "UNKNOWN";
}

const char* RiskEngine::sortStatusName(SortStatus status) const {
    switch (status) {
    case SortStatus::NORMAL_SORT:  return "NORMAL_SORT";
    case SortStatus::WRONG_SORT:   return "WRONG_SORT";
    case SortStatus::CANNOT_JUDGE: return "CANNOT_JUDGE";
    }
    return "UNKNOWN";
}
