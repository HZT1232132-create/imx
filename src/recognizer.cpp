#include "recognizer.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <vector>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <zbar.h>

// Shared helper: extract PKG-pattern package ID from raw OCR text.
// Collects all PKGxxx candidates and prefers pure-digit suffixes
// (e.g. PKG003 over PKGO03) so the OCR corrector receives the best input.
static std::string extractPackageId(const std::string& raw) {
    std::string up;
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            up += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        } else {
            up += ' ';
        }
    }

    std::regex re("PKG[0-9OIL]{3,}");

    std::vector<std::string> candidates;
    auto begin = std::sregex_iterator(up.begin(), up.end(), re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        candidates.push_back(it->str());
    }

    if (candidates.empty()) return "";

    // Prefer candidates whose suffix (after "PKG") is all digits
    for (const auto& c : candidates) {
        std::string suffix = c.substr(3);
        bool allDigits = true;
        for (char ch : suffix) {
            if (ch < '0' || ch > '9') {
                allDigits = false;
                break;
            }
        }
        if (allDigits) return c;
    }

    // Fallback: return first candidate (will be normalized by corrector)
    return candidates[0];
}

// ---- PrimaryRecognizer ----

PrimaryRecognizer::PrimaryRecognizer() = default;

RecognizeResult PrimaryRecognizer::recognize(const cv::Mat& image) {
    RecognizeResult result;
    if (image.empty()) {
        result.success = false;
        result.method = "NONE";
        return result;
    }

    std::string qrData = m_qrDetector.detectAndDecode(image);
    if (!qrData.empty()) {
        result.success = true;
        result.rawText = qrData;
        result.packageId = extractPackageId(qrData);
        if (result.packageId.empty()) {
            std::string cleaned;
            for (char c : qrData) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    cleaned += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
            }
            result.packageId = cleaned;
        }
        result.method = "QR";
        return result;
    }

    // OpenCV QR failed — try ZBar fallback
    try {
        cv::Mat gray;
        if (image.channels() == 3)
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        else
            gray = image.clone();

        zbar::Image zbarImg(gray.cols, gray.rows, "Y800", gray.data, gray.cols * gray.rows);
        zbar::ImageScanner scanner;
        scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);
        scanner.scan(zbarImg);

        for (auto it = zbarImg.symbol_begin(); it != zbarImg.symbol_end(); ++it) {
            if (it->get_type() == zbar::ZBAR_QRCODE) {
                std::string data = it->get_data();
                result.success = true;
                result.rawText = data;
                result.packageId = extractPackageId(data);
                if (result.packageId.empty()) {
                    std::string cleaned;
                    for (char c : data) {
                        if (std::isalnum(static_cast<unsigned char>(c)))
                            cleaned += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    result.packageId = cleaned;
                }
                result.method = "QR_ZBAR";
                return result;
            }
        }
    } catch (...) {
        // ZBar failed silently, fall through to OCR
    }

    result.success = false;
    result.method = "NONE";
    return result;
}

// ---- OCRRecognizer ----

OCRRecognizer::~OCRRecognizer() {
    if (m_tessApi) {
        auto* api = static_cast<tesseract::TessBaseAPI*>(m_tessApi);
        api->End();
        delete api;
        m_tessApi = nullptr;
    }
}

OCRRecognizer::OCRRecognizer() {
    auto* api = new tesseract::TessBaseAPI();
    if (api->Init(nullptr, "eng")) {
        delete api;
        m_tessApi = nullptr;
    } else {
        // Restrict to characters that appear in package IDs
        api->SetVariable("tessedit_char_whitelist", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
        api->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
        m_tessApi = api;
    }
}

RecognizeResult OCRRecognizer::recognize(const cv::Mat& image) {
    RecognizeResult result;
    result.method = "OCR";

    if (!m_tessApi || image.empty()) {
        result.success = false;
        return result;
    }

    auto* api = static_cast<tesseract::TessBaseAPI*>(m_tessApi);
    int w = image.cols;
    int h = image.rows;

    // Multi-ROI: try different regions, first PKG-match wins
    std::vector<cv::Mat> rois;

    // ROI 1: full image
    rois.push_back(image.clone());

    // ROI 2: bottom 15% (label area)
    int bottomH = h * 15 / 100;
    if (bottomH > 30) {
        rois.push_back(image(cv::Rect(0, h - bottomH, w, bottomH)).clone());
    }

    // ROI 3: top 12% (ID / title line)
    int topH = h * 12 / 100;
    if (topH > 30) {
        rois.push_back(image(cv::Rect(0, 0, w, topH)).clone());
    }

    // ROI 4: right half
    int halfW = w / 2;
    if (halfW > 50) {
        rois.push_back(image(cv::Rect(halfW, 0, w - halfW, h)).clone());
    }

    // ROI 5: bottom-right quadrant
    int qx = w * 2 / 3;
    int qy = h * 2 / 3;
    if (qx < w - 20 && qy < h - 20) {
        rois.push_back(image(cv::Rect(qx, qy, w - qx, h - qy)).clone());
    }

    for (const auto& roi : rois) {
        cv::Mat processed = preprocess(roi);
        api->SetImage(processed.data, processed.cols, processed.rows,
                      1, static_cast<int>(processed.step));

        char* outText = api->GetUTF8Text();
        if (outText) {
            std::string raw(outText);
            delete[] outText;

            std::string pkgId = extractPackageId(raw);
            if (!pkgId.empty()) {
                result.rawText = raw;
                result.packageId = pkgId;
                result.success = true;
                return result;
            }

            if (result.rawText.empty()) {
                result.rawText = raw;
            } else {
                result.rawText += " | " + raw;
            }
        }
    }

    // Final fallback: try extracting from all accumulated text
    result.packageId = extractPackageId(result.rawText);
    result.success = !result.packageId.empty();
    return result;
}

cv::Mat OCRRecognizer::preprocess(const cv::Mat& image) {
    cv::Mat gray, scaled, binary;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    // Scale up if too small
    if (gray.cols < 300 || gray.rows < 150) {
        double scale = std::min(600.0 / gray.cols, 300.0 / gray.rows);
        cv::resize(gray, scaled, cv::Size(), scale, scale, cv::INTER_CUBIC);
    } else {
        scaled = gray;
    }

    cv::adaptiveThreshold(scaled, binary, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, 15, 8);
    return binary;
}
