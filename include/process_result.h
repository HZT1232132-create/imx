#pragma once

#include <string>
#include "risk_engine.h"

struct ProcessResult {
    std::string imageName;
    std::string sceneDesc;       // from demo_sequence.csv
    std::string rawText;
    std::string finalPackageId;
    std::string targetZone;
    std::string currentZone;
    IdStatus idStatus;
    SortStatus sortStatus;
    RiskLevel riskLevel;
    std::string recognitionMethod;
    int processTimeMs = 0;
    std::string message;

    // V2 field (was missing from result struct)
    int editDistance = -1;

    // V3 quality gate fields
    double qualityScore = 0.0;    // composite 0-1
    std::string qualityLevel;     // GOOD / WARNING / BAD
    double confidence = 0.0;      // fused confidence 0-1
    std::string action;           // PASS / PASS_WITH_LOG / REVIEW / BLOCK
    std::string decisionReason;
};
