#pragma once

#include <string>
#include <map>
#include "ocr_corrector.h"

class RuleEngine {
public:
    explicit RuleEngine(const std::string& rulesPath);

    std::string getTargetZone(const std::string& packageId) const;
    const std::map<std::string, PackageRule>& getRules() const;
    bool hasPackage(const std::string& packageId) const;

private:
    std::map<std::string, PackageRule> m_rules;
    void loadRules(const std::string& path);
};
