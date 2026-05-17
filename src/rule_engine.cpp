#include "rule_engine.h"
#include <fstream>
#include <sstream>

RuleEngine::RuleEngine(const std::string& rulesPath) {
    loadRules(rulesPath);
}

std::string RuleEngine::getTargetZone(const std::string& packageId) const {
    auto it = m_rules.find(packageId);
    if (it != m_rules.end()) {
        return it->second.targetZone;
    }
    return "UNKNOWN";
}

const std::map<std::string, PackageRule>& RuleEngine::getRules() const {
    return m_rules;
}

bool RuleEngine::hasPackage(const std::string& packageId) const {
    return m_rules.find(packageId) != m_rules.end();
}

void RuleEngine::loadRules(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    // skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string id, name, zone, type;

        std::getline(ss, id, ',');
        std::getline(ss, name, ',');
        std::getline(ss, zone, ',');
        std::getline(ss, type, ',');

        // trim \r
        auto trim = [](std::string& s) {
            if (!s.empty() && s.back() == '\r') s.pop_back();
        };
        trim(id); trim(name); trim(zone); trim(type);

        PackageRule rule;
        rule.packageId = id;
        rule.name = name;
        rule.targetZone = zone;
        rule.type = type;
        m_rules[id] = rule;
    }
}
