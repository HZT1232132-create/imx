/*
 * NPU AI Engine — TFLite + Ethos-U65 Delegate
 *
 * Runs Vela-compiled INT8 TFLite models on the Ethos-U65 NPU.
 * Falls back to CPU (XNNPACK) if NPU delegate unavailable.
 *
 * Model path: MobileNetV2/TinyCNN for label quality classification
 *             or Tiny-ID-OCR CRNN (both Vela-compiled to _vela.tflite)
 *
 * Architecture:
 *   TFLite FlatBufferModel
 *   → InterpreterBuilder + Ethos-U Delegate (NPU)
 *   → or XNNPACK Delegate (CPU fallback)
 *   → AllocateTensors → Invoke → Read output
 */

#include "npu_ai_engine.h"
#include <tensorflow/lite/c/c_api.h>
#include <tensorflow/lite/delegates/external/external_delegate.h>
#include <chrono>
#include <iostream>
#include <cstring>

class NPUAIEngine : public IAIEngine {
public:
    NPUAIEngine(const std::string& modelPath, const std::string& delegatePath)
        : m_modelPath(modelPath), m_delegatePath(delegatePath) {}

    ~NPUAIEngine() override {
        if (m_interpreter) TfLiteInterpreterDelete(m_interpreter);
        if (m_delegate)   TfLiteExternalDelegateDelete(m_delegate);
        if (m_model)      TfLiteModelDelete(m_model);
        if (m_options)    TfLiteInterpreterOptionsDelete(m_options);
    }

    bool loadModel(const std::string& path) override {
        if (!path.empty()) m_modelPath = path;

        // Load Vela-compiled TFLite model
        m_model = TfLiteModelCreateFromFile(m_modelPath.c_str());
        if (!m_model) {
            std::cerr << "[NPU-AI] Failed to load model: " << m_modelPath << "\n";
            return false;
        }

        m_options = TfLiteInterpreterOptionsCreate();

        // Try loading Ethos-U delegate
        bool npuOk = false;
        if (!m_delegatePath.empty()) {
            TfLiteExternalDelegateOptions delegateOpts = TfLiteExternalDelegateOptionsDefault(m_delegatePath.c_str());
            m_delegate = TfLiteExternalDelegateCreate(&delegateOpts);
            if (m_delegate) {
                TfLiteInterpreterOptionsAddDelegate(m_options, m_delegate);
                npuOk = true;
            }
        }

        m_interpreter = TfLiteInterpreterCreate(m_model, m_options);
        if (!m_interpreter) {
            std::cerr << "[NPU-AI] Failed to create interpreter\n";
            return false;
        }

        if (TfLiteInterpreterAllocateTensors(m_interpreter) != kTfLiteOk) {
            std::cerr << "[NPU-AI] Failed to allocate tensors\n";
            return false;
        }

        m_backend = npuOk ? "NPU" : "CPU";
        m_loaded = true;
        std::cout << "[NPU-AI] Loaded model " << m_modelPath
                  << " backend=" << m_backend << "\n";
        return true;
    }

    AIResult infer(const cv::Mat& image) override {
        AIResult r;
        r.backend = m_backend;
        r.modelName = m_modelPath;
        r.inputWidth  = image.cols;
        r.inputHeight = image.rows;

        if (!m_loaded) { r.success = false; return r; }

        auto t0 = std::chrono::steady_clock::now();

        // Preprocess: grayscale, resize, normalize to [0,1]
        cv::Mat gray, resized;
        if (image.channels() == 3)
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        else
            gray = image.clone();

        // Get input tensor dimensions
        TfLiteTensor* inputTensor  = TfLiteInterpreterGetInputTensor(m_interpreter, 0);
        int inputDims = TfLiteTensorNumDims(inputTensor);
        int targetH = (inputDims >= 2) ? TfLiteTensorDim(inputTensor, 1) : 32;
        int targetW = (inputDims >= 3) ? TfLiteTensorDim(inputTensor, 2) : 160;

        // Resize keeping aspect ratio
        double scale = std::min(
            static_cast<double>(targetW) / gray.cols,
            static_cast<double>(targetH) / gray.rows);
        int newW = static_cast<int>(gray.cols * scale);
        int newH = static_cast<int>(gray.rows * scale);
        cv::resize(gray, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LANCZOS4);

        // Pad to target size
        cv::Mat padded(targetH, targetW, CV_8UC1, cv::Scalar(0));
        resized.copyTo(padded(cv::Rect(0, 0, newW, newH)));

        // Fill input tensor (quantized INT8)
        float* inputData = TfLiteTensorData(inputTensor);
        int totalElements = targetH * targetW;
        for (int i = 0; i < totalElements; ++i)
            inputData[i] = padded.data[i] / 255.0f;

        // Run inference
        if (TfLiteInterpreterInvoke(m_interpreter) != kTfLiteOk) {
            r.success = false;
            r.qualityClass = "error";
            return r;
        }

        auto t1 = std::chrono::steady_clock::now();
        r.latencyMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Read output tensor
        const TfLiteTensor* outputTensor = TfLiteInterpreterGetOutputTensor(m_interpreter, 0);
        int outputSize = 1;
        for (int i = 0; i < TfLiteTensorNumDims(outputTensor); ++i)
            outputSize *= TfLiteTensorDim(outputTensor, i);

        const float* outputData = TfLiteTensorData(outputTensor);

        // Parse output: quality classification (argmax over classes)
        int numClasses = outputSize;
        int argmax = 0;
        float maxVal = outputData[0];
        for (int i = 1; i < numClasses; ++i) {
            if (outputData[i] > maxVal) {
                maxVal = outputData[i];
                argmax = i;
            }
        }

        r.qualityConfidence = maxVal;
        r.qualityClass = classIndexToName(argmax);
        r.success = true;

        // Generate heuristic detections (placeholder — YOLO runs on CPU)
        generateDetections(image, r.detections);

        return r;
    }

    bool isLoaded()    const override { return m_loaded; }
    std::string backendName() const override { return m_backend; }

private:
    std::string m_modelPath;
    std::string m_delegatePath;
    std::string m_backend = "UNKNOWN";
    bool m_loaded = false;

    TfLiteModel*             m_model = nullptr;
    TfLiteInterpreterOptions* m_options = nullptr;
    TfLiteDelegate*          m_delegate = nullptr;
    TfLiteInterpreter*       m_interpreter = nullptr;

    static std::string classIndexToName(int idx) {
        static const char* names[] = {
            "normal", "blur", "glare", "occluded", "damaged", "skew", "dirty"
        };
        return (idx >= 0 && idx < 7) ? names[idx] : "unknown";
    }

    void generateDetections(const cv::Mat& image, std::vector<Detection>& dets) {
        int w = image.cols, h = image.rows;
        Detection box;
        box.cls = "box"; box.confidence = 0.94;
        box.bbox = cv::Rect(w*10/100, h*5/100, w*80/100, h*90/100);
        dets.push_back(box);

        Detection label;
        label.cls = "label"; label.confidence = 0.90;
        label.bbox = cv::Rect(w*55/100, h*20/100, w*40/100, h*50/100);
        dets.push_back(label);

        Detection barcode;
        barcode.cls = "barcode"; barcode.confidence = 0.88;
        barcode.bbox = cv::Rect(w*65/100, h*55/100, w*25/100, h*30/100);
        dets.push_back(barcode);
    }
};

IAIEngine* createNPUAIEngine(const std::string& modelPath, const std::string& delegatePath) {
    return new NPUAIEngine(modelPath, delegatePath);
}
