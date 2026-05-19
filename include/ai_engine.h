#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

// NPU 检测结果 — 一个标签/条码/文字区域
struct Detection {
    std::string cls;          // "box" / "label" / "barcode" / "qrcode" / "text_region" / "damage"
    double confidence = 0.0;
    cv::Rect bbox;            // x, y, w, h
    std::string labelText;    // OCR 结果 (text_region)
};

// AI 推理结果 — NPU 或 CPU 返回的统一结构
struct AIResult {
    std::string backend;      // "NPU" / "CPU" / "MOCK"
    std::string modelName;    // mobilenetv2_quality_int8_vela.tflite / yolo11n_int8_vela.tflite
    int inputWidth  = 0;
    int inputHeight = 0;
    double latencyMs = 0.0;

    // Quality classification (MobileNetV2/TinyCNN 输出)
    std::string qualityClass; // "normal" / "blur" / "glare" / "occluded" / "damaged" / "skew" / "dirty"
    double qualityConfidence = 0.0;

    // Detection results (YOLO 输出)
    std::vector<Detection> detections;

    // Anomaly score (AutoEncoder 输出, 可选)
    double anomalyScore = 0.0;

    bool success = false;
};

// AI 推理引擎抽象接口
class IAIEngine {
public:
    virtual ~IAIEngine() = default;
    virtual bool loadModel(const std::string& path) = 0;
    virtual AIResult infer(const cv::Mat& image) = 0;
    virtual bool isLoaded() const = 0;
    virtual std::string backendName() const = 0;
};
