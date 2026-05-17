#pragma once

#include <string>

struct DecisionResult {
    // Intermediate scores (5-factor fusion per 细化方案)
    double qualityScore = 0.0;         // 0-1
    double recognitionScore = 0.0;     // 0-1
    double ruleScore = 0.0;            // 0-1
    double correctionScore = 0.0;      // 0-1, reliability of correction
    double anomalyScore = 0.0;         // 0-1, inverted (1-anomaly)
    double confidence = 0.0;           // fused 0-1
    std::string action;                // PASS / PASS_WITH_LOG / REVIEW / BLOCK
    std::string reason;
};

class DecisionEngine {
public:
    DecisionResult evaluate(int idStatus, int sortStatus,
                            const std::string& qualityLevel, double overallQualityScore,
                            int editDistance, double anomalyOverall);

private:
    double recognitionScore(int idStatus, int editDistance) const;
    double qualityScoreMapping(const std::string& level, double overallScore) const;
    double ruleScore(int idStatus) const;
    double correctionReliability(int idStatus, int editDistance) const;
    double anomalyInverseScore(double anomalyOverall) const;
};
