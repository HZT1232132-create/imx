#pragma once

#include <string>
#include <opencv2/opencv.hpp>

struct ModelResult {
    bool success = false;
    std::string text;        // decoded output (e.g. "PKG001")
    double confidence = 0.0; // 0-1
    int latencyMs = 0;
    std::string backend;     // "ONNX_CPU" / "TFLITE" / "NPU"
};

class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;
    virtual bool loadModel(const std::string& path) = 0;
    virtual ModelResult infer(const cv::Mat& roi) = 0;
};
