#include "anomaly_detector.h"
#include <algorithm>
#include <numeric>
#include <cmath>

AnomalyResult AnomalyDetector::detect(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 3)
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    else
        gray = image.clone();

    AnomalyResult r;
    r.tearScore   = detectTear(gray);
    r.stainScore  = detectStain(gray);
    r.overlayScore = detectOverlay(gray);
    r.overallAnomaly = std::max({r.tearScore, r.stainScore, r.overlayScore});

    // Determine dominant type
    double best = std::max({r.tearScore, r.stainScore, r.overlayScore});
    if (best < 0.5) {
        r.anomalyType = "none";
        r.isAnomalous = false;
    } else if (r.tearScore == best)
        r.anomalyType = "tear";
    else if (r.stainScore == best)
        r.anomalyType = "stain";
    else if (r.overlayScore == best)
        r.anomalyType = "overlay";
    else
        r.anomalyType = "multiple";

    if (r.overallAnomaly >= 0.5) r.isAnomalous = true;
    return r;
}

// ── Tear: median-filter residue contains large connected components ──
double AnomalyDetector::detectTear(const cv::Mat& gray) {
    cv::Mat med, diff, binary;
    cv::medianBlur(gray, med, 15);           // large kernel removes text
    cv::absdiff(gray, med, diff);            // residue = text + damage
    cv::threshold(diff, binary, 50, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Large connected components = potential tears (text components are small)
    double maxArea = 0;
    double totalArea = gray.rows * gray.cols;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area > maxArea) maxArea = area;
    }
    return std::min(maxArea / (totalArea * 0.05), 1.0);  // 5% of image = tear
}

// ── Stain: dark/bright blobs that survive median filtering ──
double AnomalyDetector::detectStain(const cv::Mat& gray) {
    cv::Mat med, diff, binary;
    cv::medianBlur(gray, med, 21);           // large kernel for stain isolation
    cv::absdiff(gray, med, diff);
    cv::threshold(diff, binary, 40, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Count blobs with area > 0.5% of image (larger than typical character)
    double minBlobArea = gray.rows * gray.cols * 0.005;
    double stainArea = 0;
    double totalArea = gray.rows * gray.cols;
    for (const auto& c : contours) {
        double a = cv::contourArea(c);
        if (a > minBlobArea) stainArea += a;
    }
    return std::min(stainArea / (totalArea * 0.1), 1.0);  // 10% of image stained
}

// ── Overlay: region with distinctly different histogram from neighbors ──
double AnomalyDetector::detectOverlay(const cv::Mat& gray) {
    int cellSize = 40;
    int cols = gray.cols / cellSize;
    int rows = gray.rows / cellSize;
    if (cols < 3 || rows < 3) return 0.0;

    // Compute mean intensity per cell
    std::vector<std::vector<double>> cellMeans(rows, std::vector<double>(cols));
    double totalMean = 0;
    int n = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            cv::Rect roi(c * cellSize, r * cellSize, cellSize, cellSize);
            roi &= cv::Rect(0, 0, gray.cols, gray.rows);
            cellMeans[r][c] = cv::mean(gray(roi)).val[0];
            totalMean += cellMeans[r][c];
            n++;
        }
    }
    if (n == 0) return 0.0;
    totalMean /= n;

    // Find cells that differ from global mean by >30 intensity levels
    int outlierCells = 0;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            if (std::abs(cellMeans[r][c] - totalMean) > 30)
                outlierCells++;

    double ratio = static_cast<double>(outlierCells) / n;
    return std::min(ratio / 0.3, 1.0);       // 30% outliers = full overlay score
}
