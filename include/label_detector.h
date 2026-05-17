#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

struct RoiResult {
    cv::Rect qrRoi;     // QR code region
    cv::Rect textRoi;   // text/label region
    cv::Rect labelRoi;  // full label region
    cv::Mat qrPatch;
    cv::Mat textPatch;
    cv::Mat labelPatch;
    bool valid = false;
};

class LabelDetector {
public:
    // V1: fixed heuristics based on label layout knowledge.
    // V2: YOLO11n-det for label/qr/text detection (future).
    RoiResult detect(const cv::Mat& image);

private:
    cv::Rect findQrRegion(const cv::Mat& gray);
    cv::Rect findTextRegion(const cv::Mat& gray, const cv::Rect& qrRoi);
};
