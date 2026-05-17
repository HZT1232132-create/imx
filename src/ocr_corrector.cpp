#include "ocr_corrector.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <vector>

void OCRCorrector::loadRules(const std::map<std::string, PackageRule>& rules) {
    m_rules = rules;
}

CorrectionResult OCRCorrector::correct(const std::string& ocrText) {
    CorrectionResult result;
    result.originalText = ocrText;

    std::string cleaned = cleanText(ocrText);
    if (cleaned.empty()) {
        result.valid = false;
        return result;
    }

    // Step 1: exact match on cleaned text → OCR_RECOVERED
    auto it = m_rules.find(cleaned);
    if (it != m_rules.end()) {
        result.valid = true;
        result.corrected = false;
        result.finalPackageId = cleaned;
        result.editDistance = 0;
        return result;
    }

    // Step 2: normalize OCR confusions (O→0, I/L→1) and retry exact match → OCR_CORRECTED
    std::string normalized = normalizeOcrConfusions(cleaned);
    if (normalized != cleaned) {
        auto it2 = m_rules.find(normalized);
        if (it2 != m_rules.end()) {
            result.valid = true;
            result.corrected = true;
            result.finalPackageId = normalized;
            result.editDistance = 0;
            return result;
        }
    }

    // Step 3: edit distance search on normalized text
    int bestDist = std::numeric_limits<int>::max();
    std::string bestId;
    int tieCount = 0;
    for (const auto& kv : m_rules) {
        int d = levenshteinDistance(normalized, kv.first);
        if (d < bestDist) {
            bestDist = d;
            bestId = kv.first;
            tieCount = 1;
        } else if (d == bestDist) {
            tieCount++;
        }
    }

    int maxDist = (normalized.length() <= 8) ? 1 : 2;
    if (tieCount == 1 && bestDist <= maxDist) {
        result.valid = true;
        result.corrected = true;
        result.finalPackageId = bestId;
        result.editDistance = bestDist;
    } else {
        result.valid = false;
        result.finalPackageId = cleaned;
        result.editDistance = bestDist;
    }

    return result;
}

std::string OCRCorrector::cleanText(const std::string& raw) {
    std::string out;
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return out;
}

std::string OCRCorrector::normalizeOcrConfusions(const std::string& cleaned) {
    if (cleaned.rfind("PKG", 0) != 0) return cleaned;
    std::string out = cleaned;
    for (size_t i = 3; i < out.size(); ++i) {
        if (out[i] == 'O') out[i] = '0';
        if (out[i] == 'I' || out[i] == 'L') out[i] = '1';
    }
    return out;
}

int OCRCorrector::levenshteinDistance(const std::string& a, const std::string& b) {
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i - 1][j] + 1,
                                 dp[i][j - 1] + 1,
                                 dp[i - 1][j - 1] + cost});
        }
    }
    return dp[n][m];
}
