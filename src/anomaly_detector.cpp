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

// ── Tear: ratio of short edge fragments to total, excluding text-like fragments ──
double AnomalyDetector::detectTear(const cv::Mat& gray) {
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return 0.0;

    int irregularFragments = 0;
    int totalFragments = contours.size();

    for (const auto& c : contours) {
        double len = cv::arcLength(c, false);
        if (len < 2) continue;  // noise

        // Text edges are straight/smooth short segments with small bounding box
        // Tear edges are irregular zig-zag patterns
        cv::Rect br = cv::boundingRect(c);
        double aspectRatio = std::max(br.width, br.height) /
                             std::max(1.0, static_cast<double>(std::min(br.width, br.height)));

        // Irregular: short segment, high aspect ratio (line-like) but not straight
        if (len < 40 && aspectRatio > 5.0) {
            // Check if it's a straight line (text edge) or jagged (tear)
            std::vector<cv::Point> approx;
            cv::approxPolyDP(c, approx, len * 0.08, false);  // 8% epsilon = high tolerance
            // Many vertices relative to length = jagged = tear
            if (approx.size() > static_cast<size_t>(len / 8))
                irregularFragments++;
        }
    }

    double ratio = static_cast<double>(irregularFragments) /
                   std::max(1, totalFragments);
    return std::min(ratio / 0.3, 1.0);  // 30% irregular = full tear
}

// ── Stain: dark/bright blobs significantly different from blurred background ──
double AnomalyDetector::detectStain(const cv::Mat& gray) {
    cv::Mat blurred, residual, binary;
    cv::GaussianBlur(gray, blurred, cv::Size(21, 21), 0);
    cv::absdiff(gray, blurred, residual);

    cv::Scalar mu, sigma;
    cv::meanStdDev(residual, mu, sigma);
    double meanResidual = mu.val[0];
    double stdResidual = sigma.val[0];

    // Stain = regions where residual is 3 sigma above mean
    cv::threshold(residual, binary, meanResidual + 3.0 * stdResidual, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double totalArea = gray.rows * gray.cols;
    double maxArea = 0;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        cv::Rect br = cv::boundingRect(c);
        double density = area / std::max(1.0, static_cast<double>(br.area()));
        if (density > 0.5 && area > maxArea) maxArea = area;
    }

    return std::min(maxArea / totalArea / 0.05, 1.0);
}

// ── Overlay: grid cell with significantly different histogram from neighbors ──
double AnomalyDetector::detectOverlay(const cv::Mat& gray) {
    int cellSize = 40;
    int cols = gray.cols / cellSize;
    int rows = gray.rows / cellSize;
    if (cols < 3 || rows < 3) return 0.0;

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

    int outlierCells = 0;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            if (std::abs(cellMeans[r][c] - totalMean) > 30)
                outlierCells++;

    return std::min(static_cast<double>(outlierCells) / (n * 0.3), 1.0);
}
