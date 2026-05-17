#pragma once

#include <string>
#include <map>

struct CorrectionResult {
    bool valid = false;
    bool corrected = false;
    std::string originalText;
    std::string finalPackageId;
    int editDistance = -1;
};

struct PackageRule {
    std::string packageId;
    std::string name;
    std::string targetZone;
    std::string type;
};

class OCRCorrector {
public:
    void loadRules(const std::map<std::string, PackageRule>& rules);

    CorrectionResult correct(const std::string& ocrText);

private:
    std::map<std::string, PackageRule> m_rules;
    std::string cleanText(const std::string& raw);
    std::string normalizeOcrConfusions(const std::string& cleaned);
    int levenshteinDistance(const std::string& a, const std::string& b);
};
