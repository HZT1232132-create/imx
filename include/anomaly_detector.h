#pragma once

#include <opencv2/opencv.hpp>
#include <string>

struct AnomalyResult {
    bool isAnomalous = false;
    double tearScore = 0.0;       // 0-1, higher = more tear
    double stainScore = 0.0;      // 0-1, higher = more stain/dirt
    double overlayScore = 0.0;    // 0-1, higher = more overlay/sticker
    double overallAnomaly = 0.0;  // composite
    std::string anomalyType;      // "none" / "tear" / "stain" / "overlay" / "multiple"
};

class AnomalyDetector {
public:
    AnomalyResult detect(const cv::Mat& image);

private:
    double detectTear(const cv::Mat& gray);
    double detectStain(const cv::Mat& gray);
    double detectOverlay(const cv::Mat& gray);
};
