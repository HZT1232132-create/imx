#include "hash_logger.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iostream>

HashLogger::HashLogger(const std::string& ruleVer, const std::string& modelVer)
    : m_ruleVersion(ruleVer), m_modelVersion(modelVer) {}

std::string HashLogger::computeSHA256(const std::string& data) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string HashLogger::serialize(const HashRecord& rec) const {
    std::ostringstream oss;
    oss << rec.eventId << "|" << rec.timestamp << "|" << rec.imageName << "|"
        << rec.finalPackageId << "|" << rec.idStatus << "|" << rec.sortStatus << "|"
        << rec.riskLevel << "|" << rec.qualityLevel << "|"
        << static_cast<int>(rec.confidence * 100) << "|"
        << rec.ruleVersion << "|" << rec.modelVersion << "|"
        << rec.prevHash;
    return oss.str();
}

HashRecord HashLogger::buildRecord(int eventId, const ProcessResult& result) {
    HashRecord rec;
    rec.eventId = eventId;
    rec.imageName = result.imageName;
    rec.finalPackageId = result.finalPackageId;
    rec.idStatus = RiskEngine().idStatusName(result.idStatus);
    rec.sortStatus = RiskEngine().sortStatusName(result.sortStatus);
    rec.riskLevel = RiskEngine().levelName(result.riskLevel);
    rec.qualityLevel = result.qualityLevel;
    rec.confidence = result.confidence;
    rec.ruleVersion = m_ruleVersion;
    rec.modelVersion = m_modelVersion;
    rec.prevHash = m_chain.empty() ? "0000000000000000000000000000000000000000000000000000000000000000"
                                   : m_chain.back().currentHash;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ts;
    ts << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    rec.timestamp = ts.str();

    // Compute hash
    rec.currentHash = computeSHA256(serialize(rec));
    return rec;
}

void HashLogger::append(const HashRecord& record) {
    m_chain.push_back(record);
}

const std::string& HashLogger::lastHash() const {
    static std::string empty;
    return m_chain.empty() ? empty : m_chain.back().currentHash;
}

bool HashLogger::verifyAll() const {
    if (m_chain.empty()) return true;
    for (size_t i = 0; i < m_chain.size(); ++i) {
        const auto& rec = m_chain[i];
        std::string expectedPrev = (i == 0)
            ? "0000000000000000000000000000000000000000000000000000000000000000"
            : m_chain[i - 1].currentHash;
        if (rec.prevHash != expectedPrev) {
            std::cerr << "[HASH] Chain broken at event " << rec.eventId
                      << ": prev_hash mismatch\n";
            return false;
        }
        // Recompute hash from serialized data
        HashLogger temp(m_ruleVersion, m_modelVersion);
        std::string serialized = temp.serialize(rec);
        std::string recomputed = temp.computeSHA256(serialized);
        if (rec.currentHash != recomputed) {
            std::cerr << "[HASH] Tamper detected at event " << rec.eventId
                      << ": hash mismatch (data may be altered!)\n";
            return false;
        }
    }
    return true;
}

void HashLogger::writeCSV(const std::string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "event_id,timestamp,image,final_package_id,id_status,sort_status,"
      << "risk_level,quality_level,confidence,rule_version,model_version,"
      << "prev_hash,current_hash\n";
    for (const auto& rec : m_chain) {
        f << rec.eventId << ","
          << rec.timestamp << ","
          << rec.imageName << ","
          << rec.finalPackageId << ","
          << rec.idStatus << ","
          << rec.sortStatus << ","
          << rec.riskLevel << ","
          << rec.qualityLevel << ","
          << static_cast<int>(rec.confidence * 100) << ","
          << rec.ruleVersion << ","
          << rec.modelVersion << ","
          << rec.prevHash << ","
          << rec.currentHash << "\n";
    }
    f.close();
}
