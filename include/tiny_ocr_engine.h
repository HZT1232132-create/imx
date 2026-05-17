#pragma once

#include "inference_engine.h"
#include <vector>
#include <string>

/**
 * Tiny-ID-OCR engine: lightweight CRNN model for PKGxxx recognition.
 *
 * Wraps ONNX Runtime inference. Model input: 1x32xW grayscale float32.
 * Model output: (T, 1, C) log-softmax logits (CTC format).
 *
 * On i.MX93 this would use TFLite + Vela/NPU.
 * Current implementation uses ONNX Runtime CPU as a portable baseline.
 */
class TinyOCREngine : public IInferenceEngine {
public:
    TinyOCREngine();
    ~TinyOCREngine() override;

    bool loadModel(const std::string& path) override;
    ModelResult infer(const cv::Mat& roi) override;

    // Preprocess: convert BGR/grayscale ROI to (1,32,W) float32 tensor
    std::vector<float> preprocess(const cv::Mat& roi, int& outWidth) const;

    // CTC greedy decode from logits tensor
    std::string decode(const float* logits, int timeSteps, int numClasses) const;

    // CTC confidence: mean of max(log_softmax) for argmax path
    double computeConfidence(const float* logits, int timeSteps, int numClasses,
                             const std::string& decoded) const;

    bool isLoaded() const { return m_loaded; }
    const std::string& charset() const { return m_charset; }

private:
    void* m_session = nullptr;    // Ort::Session* (opaque)
    void* m_env = nullptr;        // Ort::Env* (opaque)
    void* m_memoryInfo = nullptr; // Ort::MemoryInfo* (opaque)
    bool m_loaded = false;
    std::string m_charset = "PKG0123456789OIL";
    int m_imgH = 32;
};
