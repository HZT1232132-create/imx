#include "quality_engine.h"
#include <algorithm>
#include <cmath>

QualityResult QualityEngine::assess(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    QualityResult r;
    r.blurScore      = computeBlur(gray);
    r.glareScore     = computeGlare(image);
    r.angleScore     = computeAngle(gray);
    r.occlusionScore = computeOcclusion(gray);
    r.damageScore    = computeDamage(gray);

    double avgScore = (r.blurScore + r.glareScore + r.angleScore
                     + r.occlusionScore + r.damageScore) / 5.0;
    double minScore = std::min({r.blurScore, r.glareScore, r.angleScore,
                                r.occlusionScore, r.damageScore});
    r.overallScore = minScore * 0.5 + avgScore * 0.5;

    if (r.overallScore >= 0.75) {
        r.level = "GOOD";
        r.recaptureRequired = false;
    } else if (r.overallScore >= 0.50) {
        r.level = "WARNING";
        r.recaptureRequired = false;
    } else {
        r.level = "BAD";
        r.recaptureRequired = true;
    }

    return r;
}

double QualityEngine::computeBlur(const cv::Mat& gray) {
    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(lap, mean, stddev);
    double var = stddev.val[0] * stddev.val[0];
    // Variance: < 20 = blurry, > 150 = sharp
    double score = (var - 20.0) / (150.0 - 20.0);
    return std::max(0.0, std::min(1.0, score));
}

double QualityEngine::computeGlare(const cv::Mat& image) {
    if (image.channels() < 3) return 1.0;
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);
    cv::Mat& v = channels[2];
    cv::Mat& s = channels[1];

    // Glare = small bright saturated spots (not large white background)
    // Find local brightness peaks using small-kernel max filter
    cv::Mat localMax;
    cv::dilate(v, localMax, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(11, 11)));
    cv::Mat isPeak = (v == localMax) & (v > 240) & (s < 30);

    int glareCount = cv::countNonZero(isPeak);
    int total = v.rows * v.cols;
    double ratio = static_cast<double>(glareCount) / total;
    double score = 1.0 - std::min(ratio * 50.0, 1.0);  // 2% peaks = full penalty
    return std::max(0.0, score);
}

double QualityEngine::computeAngle(const cv::Mat& gray) {
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);
    std::vector<cv::Vec2f> lines;
    cv::HoughLines(edges, lines, 1, CV_PI / 180, 80);

    if (lines.empty()) return 0.6; // neutral score if no lines found

    // Collect angles, weight by their vote (rho distance from origin)
    double totalWeight = 0;
    double weightedAngleDev = 0;
    for (const auto& line : lines) {
        double theta = line[1];
        // Normalize to [0, PI/2) range
        while (theta >= CV_PI / 2.0) theta -= CV_PI / 2.0;
        while (theta < 0) theta += CV_PI / 2.0;
        // Deviation from nearest axis (0 or PI/2)
        double dev = std::min(theta, CV_PI / 2.0 - theta);
        double weight = line[0]; // rho as weight
        weightedAngleDev += dev * weight;
        totalWeight += weight;
    }
    if (totalWeight < 1e-6) return 0.6;

    double avgDev = weightedAngleDev / totalWeight;
    // Convert rad to degrees, normalize to score
    double degDev = avgDev * 180.0 / CV_PI;
    // 0 deg = perfect, > 15 deg = poor
    double score = 1.0 - std::min(degDev / 15.0, 1.0);
    return std::max(0.0, score);
}

double QualityEngine::computeOcclusion(const cv::Mat& gray) {
    int cellRows = 6, cellCols = 6;
    int cellH = gray.rows / cellRows;
    int cellW = gray.cols / cellCols;
    if (cellH < 8 || cellW < 8) return 0.8;

    int deadCells = 0;
    int totalCells = cellRows * cellCols;

    for (int r = 0; r < cellRows; ++r) {
        for (int c = 0; c < cellCols; ++c) {
            cv::Rect roi(c * cellW, r * cellH, cellW, cellH);
            cv::Mat cell = gray(roi);
            cv::Scalar mean, stddev;
            cv::meanStdDev(cell, mean, stddev);
            // Only truly uniform cells (stddev < 5, not < 15) are "dead"
            if (stddev.val[0] < 5.0) deadCells++;
        }
    }

    double ratio = static_cast<double>(deadCells) / totalCells;
    double score = 1.0 - std::min(ratio / 0.25, 1.0);  // 25% dead = full penalty
    return std::max(0.0, score);
}

double QualityEngine::computeDamage(const cv::Mat& gray) {
    // Damage = fragmented edges, detected via edge pixel connectivity.
    // A clean label has long, continuous edges; damage creates short fragments.
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    // Count short edge fragments (< 30 px) vs total fragments
    int shortFragments = 0;
    int totalFragments = contours.size();
    if (totalFragments < 5) return 1.0;

    for (const auto& c : contours) {
        if (cv::arcLength(c, false) < 30)
            shortFragments++;
    }
    double ratio = static_cast<double>(shortFragments) / totalFragments;
    // >60% short fragments = severe damage
    double score = 1.0 - std::min(ratio / 0.6, 1.0);
    return std::max(0.0, score);
}
