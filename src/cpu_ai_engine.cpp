#include "ai_engine.h"
#include "quality_engine.h"
#include "anomaly_detector.h"
#include <chrono>
#include <iostream>

#include "cpu_ai_engine.h"

// CPU AI Engine — uses OpenCV-based quality/anomaly (no NPU required)
// When NPU is available, replace with npu_ai_engine.cpp (TFLite+Delegate)
class CPUAIEngine : public IAIEngine {
public:
    bool loadModel(const std::string& path) override {
        // No model to load — uses OpenCV algorithms directly
        m_qualityEngine = QualityEngine{};
        m_anomalyDetector = AnomalyDetector{};
        m_loaded = true;
        std::cout << "[CPU-AI] Initialized (OpenCV quality + anomaly detectors)\n";
        return true;
    }

    AIResult infer(const cv::Mat& image) override {
        auto t0 = std::chrono::steady_clock::now();
        AIResult r;
        r.backend = "CPU";
        r.modelName = "opencv_quality_v1";
        r.inputWidth  = image.cols;
        r.inputHeight = image.rows;

        // Quality assessment
        QualityResult quality = m_qualityEngine.assess(image);
        r.qualityClass = qualityLevelToClass(quality.level, quality.overallScore);
        r.qualityConfidence = quality.overallScore;

        // Anomaly detection
        AnomalyResult anomaly = m_anomalyDetector.detect(image);
        r.anomalyScore = anomaly.overallAnomaly;

        // Generate synthetic detection boxes (placeholder — NPU YOLO will replace)
        generateDetections(image, quality, anomaly, r.detections);

        auto t1 = std::chrono::steady_clock::now();
        r.latencyMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        r.success = true;
        return r;
    }

    bool isLoaded() const override { return m_loaded; }
    std::string backendName() const override { return "CPU"; }

private:
    QualityEngine m_qualityEngine;
    AnomalyDetector m_anomalyDetector;
    bool m_loaded = false;

    std::string qualityLevelToClass(const std::string& level, double score) {
        if (level == "BAD") {
            if (score < 0.3) return "damaged";
            return "blur";
        }
        if (level == "WARNING") return "glare";
        return "normal";
    }

    void generateDetections(const cv::Mat& image, const QualityResult& q,
                            const AnomalyResult& a, std::vector<Detection>& dets) {
        int w = image.cols, h = image.rows;

        // Heuristic ROI detection (placeholder for YOLO/NPU)
        // Package box: near full image
        Detection box;
        box.cls = "box"; box.confidence = 0.94;
        box.bbox = cv::Rect(w*10/100, h*5/100, w*80/100, h*90/100);
        dets.push_back(box);

        // Label region: right portion
        Detection label;
        label.cls = "label"; label.confidence = q.overallScore;
        label.bbox = cv::Rect(w*55/100, h*20/100, w*40/100, h*50/100);
        dets.push_back(label);

        // Barcode/QR region: bottom-right
        Detection barcode;
        barcode.cls = "barcode"; barcode.confidence = 0.88;
        barcode.bbox = cv::Rect(w*65/100, h*55/100, w*25/100, h*30/100);
        dets.push_back(barcode);

        if (a.isAnomalous) {
            Detection damage;
            damage.cls = "damage"; damage.confidence = a.overallAnomaly;
            damage.bbox = cv::Rect(w*50/100, h*30/100, w*40/100, h*40/100);
            dets.push_back(damage);
        }
    }
};

IAIEngine* createCPUAIEngine() { return new CPUAIEngine(); }
