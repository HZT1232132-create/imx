#include "decision_engine.h"
#include "risk_engine.h"
#include <algorithm>

DecisionResult DecisionEngine::evaluate(int idStatus, int sortStatus,
                                        const std::string& qualityLevel,
                                        double overallQualityScore,
                                        int editDistance,
                                        double anomalyOverall) {
    DecisionResult r;

    r.qualityScore     = qualityScoreMapping(qualityLevel, overallQualityScore);
    r.recognitionScore = recognitionScore(idStatus, editDistance);
    r.ruleScore        = ruleScore(idStatus);
    r.correctionScore  = correctionReliability(idStatus, editDistance);
    r.anomalyScore     = anomalyInverseScore(anomalyOverall);

    // 5-factor fusion per 细化方案 section 5.7:
    // 0.25*quality + 0.25*recognition + 0.20*rule + 0.15*correction + 0.15*(1-anomaly)
    r.confidence = 0.25 * r.qualityScore
                 + 0.25 * r.recognitionScore
                 + 0.20 * r.ruleScore
                 + 0.15 * r.correctionScore
                 + 0.15 * r.anomalyScore;

    // ---- Action mapping per 细化方案 section 5.7 ----

    // WRONG_SORT always LEVEL_4_CRITICAL -> BLOCK
    if (sortStatus == static_cast<int>(SortStatus::WRONG_SORT)) {
        r.action = "BLOCK";
        r.reason = "Sort error: package sent to wrong zone";
        return r;
    }

    // LABEL_ERROR / UNKNOWN_PACKAGE -> LEVEL_3_HIGH
    if (idStatus == static_cast<int>(IdStatus::LABEL_ERROR)) {
        r.action = "BLOCK";
        r.reason = "Label completely unreadable, recapture or manual processing needed";
        return r;
    }
    if (idStatus == static_cast<int>(IdStatus::UNKNOWN_PACKAGE)) {
        r.action = "REVIEW";
        r.reason = "Unknown package ID, human verification required";
        return r;
    }

    // Confidence-based thresholds per 细化方案
    if (r.confidence < 0.50) {
        r.action = "BLOCK";
        r.reason = "Very low confidence — block for manual inspection";
        return r;  // LEVEL_3_HIGH per spec
    }

    // OCR_CORRECTED: allow passing if confidence >= 0.60
    if (idStatus == static_cast<int>(IdStatus::OCR_CORRECTED)) {
        if (r.confidence >= 0.60) {
            r.action = "PASS_WITH_LOG";
            r.reason = "OCR corrected with adequate confidence — may proceed with log";
        } else {
            r.action = "REVIEW";
            r.reason = "OCR corrected but low confidence — human review needed";
        }
        return r;
    }

    // OCR_RECOVERED -> LEVEL_1_LOW
    if (idStatus == static_cast<int>(IdStatus::OCR_RECOVERED)) {
        r.action = r.confidence >= 0.90 ? "PASS" : "PASS_WITH_LOG";
        r.reason = r.confidence >= 0.90
            ? "OCR recovered, high confidence"
            : "OCR recovered, moderate confidence — audit log";
        return r;
    }

    // QR_SUCCESS + high quality + correct sort -> LEVEL_0_NORMAL
    if (r.confidence >= 0.90 && qualityLevel == "GOOD") {
        r.action = "PASS";
        r.reason = "High confidence, quality good — automatic sorting";
    } else if (r.confidence >= 0.70) {
        r.action = "PASS_WITH_LOG";
        r.reason = "Moderate confidence — sorting with audit log";
    } else {
        r.action = "REVIEW";
        r.reason = "Low confidence — flag for human review";
    }

    return r;
}

double DecisionEngine::recognitionScore(int idStatus, int editDistance) const {
    if (idStatus == static_cast<int>(IdStatus::QR_SUCCESS))       return 1.00;
    if (idStatus == static_cast<int>(IdStatus::OCR_RECOVERED))    return 0.80;
    if (idStatus == static_cast<int>(IdStatus::OCR_CORRECTED)) {
        int dist = std::max(0, editDistance);
        return std::max(0.0, 0.65 - 0.10 * dist);
    }
    return 0.00;
}

double DecisionEngine::qualityScoreMapping(const std::string& level, double overallScore) const {
    // Per 细化方案: HIGH>=0.8, MEDIUM 0.5-0.8, LOW<0.5
    if (level == "GOOD")    return 1.00;
    if (level == "WARNING") return 0.65;  // MEDIUM
    if (level == "BAD")     return 0.35;  // LOW
    return std::max(0.0, overallScore);
}

double DecisionEngine::ruleScore(int idStatus) const {
    if (idStatus == static_cast<int>(IdStatus::QR_SUCCESS) ||
        idStatus == static_cast<int>(IdStatus::OCR_RECOVERED)) {
        return 1.00;
    }
    if (idStatus == static_cast<int>(IdStatus::OCR_CORRECTED)) {
        return 0.75;
    }
    return 0.00;
}

double DecisionEngine::correctionReliability(int idStatus, int editDistance) const {
    // Reliability of correction: exact match=1.0, edit=1=0.7, edit>=2=0.4
    if (idStatus == static_cast<int>(IdStatus::QR_SUCCESS))       return 1.00;
    if (idStatus == static_cast<int>(IdStatus::OCR_RECOVERED))    return 1.00;
    if (idStatus == static_cast<int>(IdStatus::OCR_CORRECTED)) {
        int dist = std::max(0, editDistance);
        if (dist == 0) return 0.85;  // confusion normalized
        if (dist == 1) return 0.70;
        return 0.40;
    }
    return 0.00;
}

double DecisionEngine::anomalyInverseScore(double anomalyOverall) const {
    // Invert: high anomaly -> low score
    return 1.0 - std::min(anomalyOverall, 1.0);
}
