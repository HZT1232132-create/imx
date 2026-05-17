#pragma once

#include <string>
#include <opencv2/opencv.hpp>

struct QualityResult {
    double blurScore = 0.0;       // 0-1, higher = sharper
    double glareScore = 0.0;      // 0-1, higher = less glare
    double angleScore = 0.0;      // 0-1, higher = less tilt
    double occlusionScore = 0.0;  // 0-1, higher = less occlusion
    double damageScore = 0.0;     // 0-1, higher = less damage
    double overallScore = 0.0;    // composite (minimum of all)
    std::string level;            // GOOD / WARNING / BAD
    bool recaptureRequired = false;
};

class QualityEngine {
public:
    QualityResult assess(const cv::Mat& image);

private:
    double computeBlur(const cv::Mat& gray);
    double computeGlare(const cv::Mat& image);
    double computeAngle(const cv::Mat& gray);
    double computeOcclusion(const cv::Mat& gray);
    double computeDamage(const cv::Mat& gray);
};
