#include "tiny_ocr_engine.h"
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <numeric>
#include <chrono>

TinyOCREngine::TinyOCREngine() = default;

TinyOCREngine::~TinyOCREngine() {
    if (m_session) {
        delete static_cast<Ort::Session*>(m_session);
        m_session = nullptr;
    }
    if (m_memoryInfo) {
        delete static_cast<Ort::MemoryInfo*>(m_memoryInfo);
        m_memoryInfo = nullptr;
    }
    if (m_env) {
        delete static_cast<Ort::Env*>(m_env);
        m_env = nullptr;
    }
}

bool TinyOCREngine::loadModel(const std::string& path) {
    try {
        auto* env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "TinyOCR");
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(2);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        auto* session = new Ort::Session(*env, path.c_str(), opts);

        // Verify input shape
        Ort::AllocatorWithDefaultOptions allocator;
        size_t numInputs = session->GetInputCount();
        if (numInputs < 1) return false;
        auto inputName = session->GetInputNameAllocated(0, allocator);
        auto typeInfo = session->GetInputTypeInfo(0);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        auto shape = tensorInfo.GetShape();

        // shape should be [-1, 1, 32, -1] or [1, 1, 32, 160]
        m_imgH = shape.size() >= 3 ? shape[2] : 32;

        m_env = env;
        m_session = session;
        m_memoryInfo = new Ort::MemoryInfo(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
        m_loaded = true;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<float> TinyOCREngine::preprocess(const cv::Mat& roi, int& outWidth) const {
    cv::Mat gray;
    if (roi.channels() == 3) {
        cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = roi.clone();
    }

    // Resize height to m_imgH, keep aspect ratio
    int targetH = m_imgH;
    double scale = static_cast<double>(targetH) / gray.rows;
    outWidth = std::max(targetH, static_cast<int>(gray.cols * scale));
    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(outWidth, targetH), 0, 0, cv::INTER_LANCZOS4);

    // Normalize to [0, 1]
    std::vector<float> tensor(1 * 1 * targetH * outWidth, 0.0f);
    for (int y = 0; y < targetH; ++y) {
        const uchar* row = resized.ptr<uchar>(y);
        for (int x = 0; x < outWidth; ++x) {
            tensor[y * outWidth + x] = row[x] / 255.0f;
        }
    }
    return tensor;
}

std::string TinyOCREngine::decode(const float* logits, int timeSteps, int numClasses) const {
    std::string result;
    int prev = 0; // blank
    for (int t = 0; t < timeSteps; ++t) {
        const float* stepLogits = logits + t * numClasses;
        int maxIdx = static_cast<int>(
            std::max_element(stepLogits, stepLogits + numClasses) - stepLogits);
        if (maxIdx != 0 && maxIdx != prev) {
            if (maxIdx > 0 && maxIdx <= static_cast<int>(m_charset.size())) {
                result += m_charset[maxIdx - 1];
            }
        }
        prev = maxIdx;
    }
    return result;
}

double TinyOCREngine::computeConfidence(const float* logits, int timeSteps, int numClasses,
                                        const std::string& decoded) const {
    // Mean of exp(log_softmax) at argmax positions along the decoded path.
    // Build the expected index sequence for the decoded string
    std::vector<int> targetIdx;
    for (char c : decoded) {
        auto pos = m_charset.find(c);
        if (pos != std::string::npos) {
            targetIdx.push_back(static_cast<int>(pos) + 1);
        }
    }
    if (targetIdx.empty()) return 0.0;

    // Align decoded chars to time steps (simple greedy alignment)
    double totalConf = 0.0;
    int charPos = 0;
    int prev = 0;
    for (int t = 0; t < timeSteps && charPos < static_cast<int>(targetIdx.size()); ++t) {
        const float* stepLogits = logits + t * numClasses;
        int maxIdx = static_cast<int>(
            std::max_element(stepLogits, stepLogits + numClasses) - stepLogits);
        if (maxIdx != 0 && maxIdx != prev && maxIdx == targetIdx[charPos]) {
            // exp(log_softmax) = probability at this step
            totalConf += std::exp(stepLogits[maxIdx]);
            charPos++;
        }
        prev = maxIdx;
    }

    return (charPos > 0) ? totalConf / charPos : 0.0;
}

ModelResult TinyOCREngine::infer(const cv::Mat& roi) {
    ModelResult result;
    result.backend = "ONNX_CPU";
    if (!m_loaded || roi.empty()) {
        result.success = false;
        return result;
    }

    try {
        auto t0 = std::chrono::steady_clock::now();

        auto* session = static_cast<Ort::Session*>(m_session);
        auto* memoryInfo = static_cast<Ort::MemoryInfo*>(m_memoryInfo);

        // Preprocess
        int outWidth = 0;
        std::vector<float> inputData = preprocess(roi, outWidth);
        std::vector<int64_t> inputShape = {1, 1, m_imgH, outWidth};

        // Create input tensor
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            *memoryInfo, inputData.data(), inputData.size(),
            inputShape.data(), inputShape.size());

        // Get input/output names
        Ort::AllocatorWithDefaultOptions allocator;
        auto inputName = session->GetInputNameAllocated(0, allocator);
        auto outputName = session->GetOutputNameAllocated(0, allocator);
        const char* inputNames[] = {inputName.get()};
        const char* outputNames[] = {outputName.get()};

        // Run inference
        auto outputs = session->Run(Ort::RunOptions{nullptr},
                                    inputNames, &inputTensor, 1,
                                    outputNames, 1);

        auto t1 = std::chrono::steady_clock::now();
        result.latencyMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

        // Parse output: (T, 1, C)
        const float* outputData = outputs[0].GetTensorData<float>();
        auto outputShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int T = static_cast<int>(outputShape[0]);
        int C = static_cast<int>(outputShape[2]);
        // log_softmax → exponentiate for probabilities
        // (We keep log_softmax and decode from argmax, which is invariant)

        result.text = decode(outputData, T, C);
        result.confidence = computeConfidence(outputData, T, C, result.text);
        result.success = !result.text.empty();
    } catch (const std::exception& e) {
        result.success = false;
        result.text = "";
    }
    return result;
}
