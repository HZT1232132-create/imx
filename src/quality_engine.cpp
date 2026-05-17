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

    if (r.overallScore >= 0.70) {
        r.level = "GOOD";
        r.recaptureRequired = false;
    } else if (r.overallScore >= 0.40) {
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
    if (image.channels() < 3) return 1.0; // grayscale has no glare
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);
    cv::Mat& v = channels[2];
    cv::Mat& s = channels[1];

    int glareCount = 0;
    int total = v.rows * v.cols;
    for (int y = 0; y < v.rows; ++y) {
        const uchar* rowV = v.ptr<uchar>(y);
        const uchar* rowS = s.ptr<uchar>(y);
        for (int x = 0; x < v.cols; ++x) {
            // Glare = very bright AND low saturation
            if (rowV[x] > 248 && rowS[x] < 30) glareCount++;
        }
    }
    double ratio = static_cast<double>(glareCount) / total;
    // >50% = white background (not glare); 10-50% = localized glare
    if (ratio > 0.50) return 0.85;
    double score = 1.0 - std::min(ratio * 4.0, 1.0);
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
    int cellRows = 4, cellCols = 4;
    int cellH = gray.rows / cellRows;
    int cellW = gray.cols / cellCols;
    if (cellH < 8 || cellW < 8) return 0.8; // image too small, assume OK

    int deadCells = 0;
    int totalCells = cellRows * cellCols;

    for (int r = 0; r < cellRows; ++r) {
        for (int c = 0; c < cellCols; ++c) {
            cv::Rect roi(c * cellW, r * cellH, cellW, cellH);
            cv::Mat cell = gray(roi);
            cv::Scalar mean, stddev;
            cv::meanStdDev(cell, mean, stddev);
            if (stddev.val[0] < 15.0) deadCells++;
        }
    }

    double score = 1.0 - static_cast<double>(deadCells) / totalCells;
    return std::max(0.0, std::min(1.0, score));
}

double QualityEngine::computeDamage(const cv::Mat& gray) {
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    int cellRows = 4, cellCols = 4;
    int cellH = gray.rows / cellRows;
    int cellW = gray.cols / cellCols;
    if (cellH < 8 || cellW < 8) return 0.8;

    std::vector<double> densities;
    for (int r = 0; r < cellRows; ++r) {
        for (int c = 0; c < cellCols; ++c) {
            cv::Rect roi(c * cellW, r * cellH, cellW, cellH);
            cv::Mat cell = edges(roi);
            double edgePixels = static_cast<double>(cv::countNonZero(cell));
            double density = edgePixels / (cellW * cellH);
            densities.push_back(density);
        }
    }

    // Compute mean and stddev of edge density across cells
    double sum = 0;
    for (double d : densities) sum += d;
    double mean = sum / densities.size();

    double sqSum = 0;
    for (double d : densities) sqSum += (d - mean) * (d - mean);
    double stddev = std::sqrt(sqSum / densities.size());

    // High variation in edge density = possible damage
    // Score decays as variation increases
    double score = 1.0 - std::min(stddev / 0.08, 1.0);
    return std::max(0.0, score);
}
