#pragma once

#include <string>
#include <opencv2/opencv.hpp>

struct RecognizeResult {
    bool success = false;
    std::string rawText;
    std::string packageId;
    std::string method; // "QR" / "BARCODE" / "OCR" / "NONE"
};

class PrimaryRecognizer {
public:
    PrimaryRecognizer();
    RecognizeResult recognize(const cv::Mat& image);

private:
    cv::QRCodeDetector m_qrDetector;
};

class OCRRecognizer {
public:
    OCRRecognizer();
    ~OCRRecognizer();

    RecognizeResult recognize(const cv::Mat& image);

private:
    void* m_tessApi; // Tesseract API pointer (opaque)
    cv::Mat preprocess(const cv::Mat& image);
};
