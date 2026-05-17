#include "anomaly_detector.h"
#include <algorithm>
#include <numeric>

AnomalyResult AnomalyDetector::detect(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    AnomalyResult r;
    r.tearScore = detectTear(gray);
    r.stainScore = detectStain(gray);
    r.overlayScore = detectOverlay(gray);
    r.overallAnomaly = std::max({r.tearScore, r.stainScore, r.overlayScore});

    if (r.overallAnomaly > 0.75) r.isAnomalous = true;
    if (r.tearScore >= r.stainScore && r.tearScore >= r.overlayScore && r.tearScore > 0.4)
        r.anomalyType = "tear";
    else if (r.stainScore >= r.tearScore && r.stainScore >= r.overlayScore && r.stainScore > 0.4)
        r.anomalyType = "stain";
    else if (r.overlayScore >= r.tearScore && r.overlayScore >= r.stainScore && r.overlayScore > 0.4)
        r.anomalyType = "overlay";
    else if (r.overallAnomaly > 0.4)
        r.anomalyType = "multiple";
    else
        r.anomalyType = "none";

    return r;
}

double AnomalyDetector::detectTear(const cv::Mat& gray) {
    // Tear = abrupt edge discontinuities, measured via local edge density variance.
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    int cellSize = 16;
    int cols = gray.cols / cellSize;
    int rows = gray.rows / cellSize;
    if (cols < 2 || rows < 2) return 0.0;

    std::vector<double> densities;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            cv::Rect roi(c * cellSize, r * cellSize, cellSize, cellSize);
            if (roi.x + roi.width <= gray.cols && roi.y + roi.height <= gray.rows) {
                double d = static_cast<double>(cv::countNonZero(edges(roi))) / (cellSize * cellSize);
                densities.push_back(d);
            }
        }
    }
    if (densities.empty()) return 0.0;

    double mean = std::accumulate(densities.begin(), densities.end(), 0.0) / densities.size();
    double sqSum = 0;
    for (double d : densities) sqSum += (d - mean) * (d - mean);
    double stddev = std::sqrt(sqSum / densities.size());

    // High stddev of edge density across cells = possible tear/rip
    double score = std::min(stddev / 0.15, 1.0);
    return score;
}

double AnomalyDetector::detectStain(const cv::Mat& gray) {
    // Stain = localized dark or bright blobs with sharp boundaries.
    // Use morphological gradient to find blob boundaries.
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9));
    cv::Mat opened, closed, gradient;
    cv::morphologyEx(gray, opened, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(gray, closed, cv::MORPH_CLOSE, kernel);
    gradient = closed - opened;

    cv::Scalar mean, stddev;
    cv::meanStdDev(gradient, mean, stddev);
    double score = std::min(stddev.val[0] / 25.0, 1.0);
    return score;
}

double AnomalyDetector::detectOverlay(const cv::Mat& gray) {
    // Overlay/sticker = region with different texture from surroundings.
    // Use Laplacian variance in grid cells, compare adjacent cells.
    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_64F);

    int cellSize = 32;
    int cols = gray.cols / cellSize;
    int rows = gray.rows / cellSize;
    if (cols < 2 || rows < 2) return 0.0;

    std::vector<double> lapVars;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            cv::Rect roi(c * cellSize, r * cellSize, cellSize, cellSize);
            if (roi.x + roi.width <= gray.cols && roi.y + roi.height <= gray.rows) {
                cv::Mat cell = lap(roi);
                cv::Scalar m, s;
                cv::meanStdDev(cell, m, s);
                lapVars.push_back(s.val[0] * s.val[0]);
            }
        }
    }
    if (lapVars.empty()) return 0.0;

    double mean = std::accumulate(lapVars.begin(), lapVars.end(), 0.0) / lapVars.size();
    double sqSum = 0;
    for (double v : lapVars) sqSum += (v - mean) * (v - mean);
    double stddev = std::sqrt(sqSum / lapVars.size());

    // High Laplacian variance deviation = possible overlay/sticker
    double score = std::min(stddev / 50.0, 1.0);
    return score;
}
