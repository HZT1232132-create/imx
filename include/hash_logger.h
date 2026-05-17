#pragma once

#include <string>
#include <vector>
#include "process_result.h"

struct HashRecord {
    int eventId;
    std::string timestamp;
    std::string imageName;
    std::string finalPackageId;
    std::string idStatus;
    std::string sortStatus;
    std::string riskLevel;
    std::string qualityLevel;
    double confidence;
    std::string prevHash;
    std::string currentHash;
    std::string ruleVersion;
    std::string modelVersion;
};

class HashLogger {
public:
    HashLogger(const std::string& ruleVer, const std::string& modelVer);

    HashRecord buildRecord(int eventId, const ProcessResult& result);
    void append(const HashRecord& record);
    bool verifyAll() const;

    const std::vector<HashRecord>& records() const { return m_chain; }
    const std::string& lastHash() const;

    // Write hash chain to CSV
    void writeCSV(const std::string& path) const;

private:
    std::vector<HashRecord> m_chain;
    std::string m_ruleVersion;
    std::string m_modelVersion;

    std::string computeSHA256(const std::string& data) const;
    std::string serialize(const HashRecord& record) const;
};
