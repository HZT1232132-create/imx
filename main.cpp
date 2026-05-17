#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <opencv2/opencv.hpp>

#include "input_source.h"
#include "recognizer.h"
#include "ocr_corrector.h"
#include "rule_engine.h"
#include "risk_engine.h"
#include "alarm_simulator.h"
#include "logger.h"
#include "stats.h"
#include "quality_engine.h"
#include "decision_engine.h"
#include "tiny_ocr_engine.h"
#include "hash_logger.h"
#include "anomaly_detector.h"

static void drawHUD(cv::Mat& display, const ProcessResult& res, const StatsManager& stats,
                    const RiskEngine& engine, int frameIdx, int totalFrames) {
    int w = display.cols;
    int h = display.rows;

    // Semi-transparent top overlay
    cv::Mat overlay;
    display.copyTo(overlay);
    int topH = 140;
    cv::rectangle(overlay, cv::Rect(0, 0, w, topH), cv::Scalar(20, 20, 20), cv::FILLED);
    cv::addWeighted(overlay, 0.6, display, 0.4, 0, display);

    int y = 22;
    auto put = [&](const std::string& text, const cv::Scalar& color = cv::Scalar(255, 255, 255)) {
        cv::putText(display, text, cv::Point(12, y), cv::FONT_HERSHEY_SIMPLEX,
                    0.42, color, 1, cv::LINE_AA);
        y += 18;
    };

    // Line 1: frame + image name
    put("[" + std::to_string(frameIdx) + "/" + std::to_string(totalFrames) + "] "
        + res.imageName + "  |  " + res.sceneDesc);

    // Line 2: Package info
    put("Package ID: " + res.finalPackageId + "  |  Method: "
        + std::string(engine.idStatusName(res.idStatus)));

    // Line 3: Zone + Sort
    put("Target Zone: " + res.targetZone + "  |  Current Zone: " + res.currentZone
        + "  |  Sort: " + std::string(engine.sortStatusName(res.sortStatus)));

    // Line 4: Risk level with color
    cv::Scalar riskColor = cv::Scalar(0, 255, 0);
    switch (res.riskLevel) {
    case RiskLevel::LEVEL_1_LOW:
    case RiskLevel::LEVEL_2_MEDIUM:    riskColor = cv::Scalar(0, 255, 255); break;
    case RiskLevel::LEVEL_3_HIGH:
    case RiskLevel::LEVEL_4_CRITICAL:  riskColor = cv::Scalar(0, 0, 255); break;
    default: break;
    }
    put("Risk: " + std::string(engine.levelName(res.riskLevel)) + "  |  Time: "
        + std::to_string(res.processTimeMs) + "ms", riskColor);

    // Line 5: V3 Quality Gate + Decision
    char confBuf[32];
    snprintf(confBuf, sizeof(confBuf), "%.0f%%", res.confidence * 100.0);

    cv::Scalar actionColor = cv::Scalar(0, 255, 0);
    if (res.action == "REVIEW")        actionColor = cv::Scalar(0, 255, 255);
    else if (res.action == "BLOCK")    actionColor = cv::Scalar(0, 0, 255);
    else if (res.action == "PASS_WITH_LOG") actionColor = cv::Scalar(180, 255, 255);

    put("Quality=" + res.qualityLevel + "  |  Confidence=" + std::string(confBuf)
        + "  |  Action=" + res.action + "  |  Anomaly="
        + (res.message.find("标签异常") != std::string::npos ? "DETECTED" : "none"), actionColor);

    // Line 6: Stats summary
    char statsBuf[256];
    snprintf(statsBuf, sizeof(statsBuf),
             "Stats: Total=%d  QR=%d  OCR=%d  Err=%d  Wrong=%d  Rate=%.0f%%  AvgT=%dms  "
             "P=%d PL=%d Rv=%d Bk=%d",
             stats.total(), stats.qrSuccess(), stats.recognizedCount() - stats.qrSuccess(),
             stats.labelError() + stats.unknownPackage(), stats.wrongSort(),
             stats.overallRate(), stats.avgTimeMs(),
             stats.passCount(), stats.passWithLogCount(), stats.reviewCount(), stats.blockCount());
    put(std::string(statsBuf), cv::Scalar(180, 200, 255));

    // Bottom bar
    cv::rectangle(display, cv::Rect(0, h - 35, w, 35), cv::Scalar(20, 20, 20), cv::FILLED);
    cv::putText(display, "Press any key to continue  |  'q' to quit  |  " + res.message,
                cv::Point(12, h - 10), cv::FONT_HERSHEY_SIMPLEX,
                0.4, cv::Scalar(200, 200, 200), 1, cv::LINE_AA);

    // Recent high-risk events
    const auto& events = stats.recentEvents();
    if (!events.empty()) {
        int ey = h - 45;
        for (auto it = events.rbegin(); it != events.rend() && (ey > h - 80); ++it) {
            cv::putText(display, "[!] " + *it,
                        cv::Point(12, ey), cv::FONT_HERSHEY_SIMPLEX,
                        0.35, cv::Scalar(0, 200, 255), 1, cv::LINE_AA);
            ey -= 16;
        }
    }
}

int main(int argc, char* argv[]) {
    std::string seqPath   = "../data/demo_sequence.csv";
    std::string rulesPath = "../data/rules.csv";
    std::string imageDir  = "../data/images";
    int waitMs = 0;
    std::string mode = "full"; // baseline / ocr / full

    if (argc >= 2 && argv[1][0] != '\0') seqPath   = argv[1];
    if (argc >= 3 && argv[2][0] != '\0') rulesPath = argv[2];
    if (argc >= 4 && argv[3][0] != '\0') imageDir  = argv[3];
    if (argc >= 5) waitMs    = std::atoi(argv[4]);
    if (argc >= 6) mode      = argv[5];

    // Ensure logs directory exists
    std::filesystem::create_directories("../logs");

    // Ensure images directory exists (for writable output if needed)
    std::filesystem::create_directories(imageDir);

    std::cout << "========================================\n";
    std::cout << " i.MX93 仓储标签异常鲁棒识别与分拣告警模拟系统\n";
    // Auto-detect headless: skip GUI when no display available
    bool headless = (std::getenv("DISPLAY") == nullptr) && (std::getenv("WAYLAND_DISPLAY") == nullptr);
    std::cout << " Mode: " << mode << (headless ? " (headless)" : "") << "\n";
    std::cout << "========================================\n\n";

    // 1. Load rules
    std::cout << "[INIT] Loading rules from " << rulesPath << "...\n";
    RuleEngine ruleEngine(rulesPath);
    std::cout << "[INIT] Loaded " << ruleEngine.getRules().size() << " rules.\n";

    // 2. Load demo sequence
    std::cout << "[INIT] Loading demo sequence from " << seqPath << "...\n";
    InputSource inputSource(seqPath, imageDir);
    std::cout << "[INIT] Loaded " << inputSource.size() << " frames.\n";

    // 3. Init recognizers
    std::cout << "[INIT] Initializing QRCodeDetector...\n";
    PrimaryRecognizer primaryRec;

    std::cout << "[INIT] Initializing Tesseract OCR...\n";
    OCRRecognizer ocrRec;

    // 3b. Init Tiny-ID-OCR (V4 lightweight model)
    TinyOCREngine tinyOCR;
    if (mode == "tinyocr") {
        std::string modelPath = "../models/tiny_id_ocr.onnx";
        if (!std::filesystem::exists(modelPath)) {
            modelPath = "../runs/tiny_id_ocr_v1/tiny_id_ocr.onnx";
        }
        std::cout << "[INIT] Loading Tiny-ID-OCR model from " << modelPath << "...\n";
        if (tinyOCR.loadModel(modelPath)) {
            std::cout << "[INIT] Tiny-ID-OCR loaded successfully.\n";
        } else {
            std::cerr << "[WARN] Failed to load Tiny-ID-OCR model, falling back to Tesseract.\n";
        }
    }

    // 4. Init OCR corrector
    OCRCorrector corrector;
    corrector.loadRules(ruleEngine.getRules());

    // 5. Init engines
    RiskEngine riskEngine;
    AlarmSimulator alarm;
    EventLogger logger("../logs/events.csv");
    if (!logger.isOpen()) {
        std::cerr << "[WARN] Cannot open log file: ../logs/events.csv\n";
    }
    StatsManager stats;
    QualityEngine qualityEngine;
    DecisionEngine decisionEngine;
    HashLogger hashLogger("V5.0", "TinyOCR-V1");
    AnomalyDetector anomalyDetector;

    std::cout << "[INIT] All modules initialized.\n\n";
    std::cout << "----------------------------------------\n";

    int frameIdx = 0;
    int totalFrames = static_cast<int>(inputSource.size());

    while (inputSource.hasNext()) {
        frameIdx++;
        InputFrame frame = inputSource.next();

        auto start = std::chrono::steady_clock::now();

        std::cout << "\n--- Frame " << frameIdx << "/" << totalFrames
                  << ": " << frame.imageName << " ---\n";
        std::cout << "Scene: " << frame.sceneDesc << "\n";

        // Read image
        cv::Mat image = cv::imread(frame.imagePath);
        if (image.empty()) {
            std::cerr << "[ERROR] Cannot read image: " << frame.imagePath << "\n";
            continue;
        }

        ProcessResult result;
        result.imageName = frame.imageName;
        result.sceneDesc = frame.sceneDesc;
        result.currentZone = frame.currentZone;
        result.idStatus = IdStatus::LABEL_ERROR;      // default
        result.sortStatus = SortStatus::CANNOT_JUDGE;
        result.finalPackageId = "UNKNOWN";
        result.targetZone = "UNKNOWN";
        result.recognitionMethod = "NONE";

        // ================================================================
        // Stage 0: Quality Assessment (V3)
        // ================================================================
        QualityResult quality = qualityEngine.assess(image);
        result.qualityLevel = quality.level;
        result.qualityScore = quality.overallScore;
        std::cout << "Quality: " << quality.level
                  << " (blur=" << quality.blurScore
                  << " glare=" << quality.glareScore
                  << " angle=" << quality.angleScore
                  << " occl=" << quality.occlusionScore
                  << " dmg=" << quality.damageScore << ")\n";

        // ================================================================
        // Stage 0.5: Anomaly Detection (V5)
        // ================================================================
        AnomalyResult anomaly = anomalyDetector.detect(image);
        if (anomaly.isAnomalous) {
            std::cout << "Anomaly: " << anomaly.anomalyType
                      << " (tear=" << anomaly.tearScore
                      << " stain=" << anomaly.stainScore
                      << " overlay=" << anomaly.overlayScore << ")\n";
            result.message = "标签异常检测: " + anomaly.anomalyType;
        }

        // ================================================================
        // Stage 1: Primary Recognition (QR)
        // ================================================================
        RecognizeResult rec = primaryRec.recognize(image);

        if (rec.success) {
            result.rawText = rec.rawText;
            result.recognitionMethod = "QR";

            if (ruleEngine.hasPackage(rec.packageId)) {
                // QR decoded a known package ID → QR_SUCCESS
                result.idStatus = IdStatus::QR_SUCCESS;
                result.finalPackageId = rec.packageId;
                std::cout << "Recognition: QR SUCCESS → " << rec.packageId << "\n";
            } else {
                // QR decoded text but not in rules → try correction
                std::cout << "Recognition: QR got \"" << rec.packageId
                          << "\" not in rules, trying correction...\n";

                if (mode != "baseline") {
                    CorrectionResult corr = corrector.correct(rec.packageId);
                    if (corr.valid) {
                        result.idStatus = corr.corrected ? IdStatus::OCR_CORRECTED
                                                         : IdStatus::OCR_RECOVERED;
                        result.finalPackageId = corr.finalPackageId;
                        result.editDistance = corr.editDistance;
                        std::cout << "  Correction: \"" << rec.packageId
                                  << "\" → \"" << corr.finalPackageId
                                  << "\" (dist=" << corr.editDistance << ")\n";
                    } else {
                        result.idStatus = IdStatus::UNKNOWN_PACKAGE;
                        result.finalPackageId = rec.packageId;
                        std::cout << "  Correction: no match → UNKNOWN_PACKAGE\n";
                    }
                } else {
                    // Baseline mode: treat as unknown
                    result.idStatus = IdStatus::UNKNOWN_PACKAGE;
                    result.finalPackageId = rec.packageId;
                    std::cout << "  Baseline mode → UNKNOWN_PACKAGE\n";
                }
            }
        } else {
            // QR failed
            std::cout << "Recognition: QR FAILED\n";

            if (mode == "baseline") {
                // Baseline: no OCR, just label error
                result.idStatus = IdStatus::LABEL_ERROR;
                result.rawText = "";
                result.recognitionMethod = "NONE";
                std::cout << "  Baseline mode → LABEL_ERROR\n";
            } else if (mode == "tinyocr" && tinyOCR.isLoaded()) {
                // ---- Stage 2: Tiny-ID-OCR Fallback (V4) with Multi-ROI ----
                result.recognitionMethod = "TINYOCR";

                // Build ROI list (same multi-ROI strategy as Tesseract)
                int rw = image.cols;
                int rh = image.rows;
                std::vector<cv::Mat> rois;
                rois.push_back(image.clone());
                int bh = rh * 15 / 100;
                int th = rh * 12 / 100;
                int hw = rw / 2;
                int qx = rw * 2 / 3, qy = rh * 2 / 3;
                if (bh > 30) rois.push_back(image(cv::Rect(0, rh - bh, rw, bh)).clone());
                if (th > 30) rois.push_back(image(cv::Rect(0, 0, rw, th)).clone());
                if (hw > 50) rois.push_back(image(cv::Rect(hw, 0, rw - hw, rh)).clone());
                if (qx < rw - 20 && qy < rh - 20)
                    rois.push_back(image(cv::Rect(qx, qy, rw - qx, rh - qy)).clone());

                ModelResult bestTiny;
                bool found = false;
                for (const auto& roi : rois) {
                    // Preprocess: grayscale → resize (keep aspect ratio)
                    // TinyOCR expects grayscale white-on-black [0,1], same as training.
                    // Scene images (black text on white) need inversion; we try both.
                    cv::Mat gray, scaled, inverted;
                    if (roi.channels() == 3) cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
                    else gray = roi.clone();
                    if (gray.cols < 100 || gray.rows < 32) {
                        double s = std::max(32.0 / gray.rows, 100.0 / gray.cols);
                        cv::resize(gray, scaled, cv::Size(), s, s, cv::INTER_CUBIC);
                    } else {
                        scaled = gray;
                    }
                    cv::bitwise_not(scaled, inverted);

                    // Try native first (white-on-black), then inverted
                    ModelResult tiny = tinyOCR.infer(scaled);
                    std::string pkgId;
                    for (size_t i = 0; i + 3 <= tiny.text.size(); ++i) {
                        if (tiny.text.substr(i, 3) == "PKG") { pkgId = tiny.text.substr(i); break; }
                    }
                    if (pkgId.empty()) pkgId = tiny.text;
                    if (!tiny.success || pkgId.size() < 5) {
                        tiny = tinyOCR.infer(inverted);
                        pkgId.clear();
                        for (size_t i = 0; i + 3 <= tiny.text.size(); ++i) {
                            if (tiny.text.substr(i, 3) == "PKG") { pkgId = tiny.text.substr(i); break; }
                        }
                        if (pkgId.empty()) pkgId = tiny.text;
                    }
                    if (!tiny.success || pkgId.size() < 5) continue;
                    tiny.text = pkgId;
                    bestTiny = tiny;
                    found = true;
                    break;
                }

                if (!found) {
                    result.idStatus = IdStatus::LABEL_ERROR;
                    result.rawText = "";
                    std::cout << "  TinyOCR: no PKGxxx found → LABEL_ERROR\n";
                } else {
                    result.rawText = bestTiny.text;
                    std::cout << "  TinyOCR: \"" << bestTiny.text
                              << "\" (conf=" << bestTiny.confidence
                              << " latency=" << bestTiny.latencyMs << "ms)\n";

                    CorrectionResult corr = corrector.correct(bestTiny.text);
                    if (corr.valid && !corr.corrected) {
                        result.idStatus = IdStatus::OCR_RECOVERED;
                        result.finalPackageId = corr.finalPackageId;
                        result.editDistance = corr.editDistance;
                        std::cout << "  TinyOCR Correction: exact match → OCR_RECOVERED\n";
                    } else if (corr.valid && corr.corrected) {
                        result.idStatus = IdStatus::OCR_CORRECTED;
                        result.finalPackageId = corr.finalPackageId;
                        result.editDistance = corr.editDistance;
                        std::cout << "  TinyOCR Correction: edit=" << corr.editDistance
                                  << " → \"" << corr.finalPackageId << "\" → OCR_CORRECTED\n";
                    } else {
                        result.idStatus = IdStatus::UNKNOWN_PACKAGE;
                        result.finalPackageId = bestTiny.text;
                        std::cout << "  TinyOCR Correction: no match → UNKNOWN_PACKAGE\n";
                    }
                }
            } else {
                // ---- Stage 2: OCR Fallback ----
                result.recognitionMethod = "OCR";
                RecognizeResult ocr = ocrRec.recognize(image);

                if (!ocr.success || ocr.packageId.empty()) {
                    result.idStatus = IdStatus::LABEL_ERROR;
                    result.rawText = ocr.rawText;
                    std::cout << "  OCR: FAILED → LABEL_ERROR\n";
                } else {
                    result.rawText = ocr.rawText;
                    std::cout << "  OCR raw: \"" << ocr.rawText
                              << "\" → extracted: \"" << ocr.packageId << "\"\n";

                    // ---- Stage 3: OCR Correction ----
                    if (mode == "ocr") {
                        // OCR mode: only exact match, no correction
                        if (ruleEngine.hasPackage(ocr.packageId)) {
                            result.idStatus = IdStatus::OCR_RECOVERED;
                            result.finalPackageId = ocr.packageId;
                            std::cout << "  OCR mode: exact match → OCR_RECOVERED\n";
                        } else {
                            result.idStatus = IdStatus::UNKNOWN_PACKAGE;
                            result.finalPackageId = ocr.packageId;
                            std::cout << "  OCR mode: no match → UNKNOWN_PACKAGE\n";
                        }
                    } else {
                        // Full mode: correction enabled
                        CorrectionResult corr = corrector.correct(ocr.packageId);
                        if (corr.valid && !corr.corrected) {
                            result.idStatus = IdStatus::OCR_RECOVERED;
                            result.finalPackageId = corr.finalPackageId;
                            result.editDistance = corr.editDistance;
                            std::cout << "  OCR Correction: exact match → OCR_RECOVERED\n";
                        } else if (corr.valid && corr.corrected) {
                            result.idStatus = IdStatus::OCR_CORRECTED;
                            result.finalPackageId = corr.finalPackageId;
                            result.editDistance = corr.editDistance;
                            std::cout << "  OCR Correction: edit=" << corr.editDistance
                                      << " → \"" << corr.finalPackageId << "\" → OCR_CORRECTED\n";
                        } else {
                            result.idStatus = IdStatus::UNKNOWN_PACKAGE;
                            result.finalPackageId = ocr.packageId;
                            std::cout << "  OCR Correction: no match (dist=" << corr.editDistance
                                      << ") → UNKNOWN_PACKAGE\n";
                        }
                    }
                }
            }
        }

        // ================================================================
        // Stage 4: Sort Judgment
        // ================================================================
        if (result.idStatus == IdStatus::LABEL_ERROR ||
            result.idStatus == IdStatus::UNKNOWN_PACKAGE) {
            result.sortStatus = SortStatus::CANNOT_JUDGE;
            result.targetZone = "UNKNOWN";
            result.message = "无法判断分拣状态，需人工处理";
        } else {
            result.targetZone = ruleEngine.getTargetZone(result.finalPackageId);
            if (result.targetZone == result.currentZone) {
                result.sortStatus = SortStatus::NORMAL_SORT;
                result.message = "正确分拣";
            } else {
                result.sortStatus = SortStatus::WRONG_SORT;
                result.message = "错误：货物应去 " + result.targetZone
                               + " 区，当前在 " + result.currentZone + " 区";
            }
        }
        std::cout << "Sort: target=" << result.targetZone
                  << " current=" << result.currentZone
                  << " → " << riskEngine.sortStatusName(result.sortStatus) << "\n";

        // ================================================================
        // Stage 5: Risk Level
        // ================================================================
        result.riskLevel = riskEngine.map(result.idStatus, result.sortStatus);
        std::cout << "Risk: " << riskEngine.levelName(result.riskLevel) << "\n";

        // ================================================================
        // Stage 5.5: Decision Engine (V3 — confidence fusion + action)
        // ================================================================
        DecisionResult decision = decisionEngine.evaluate(
            static_cast<int>(result.idStatus),
            static_cast<int>(result.sortStatus),
            result.qualityLevel,
            result.qualityScore,
            result.editDistance,
            anomaly.overallAnomaly);
        result.confidence = decision.confidence;
        result.action = decision.action;
        result.decisionReason = decision.reason;
        std::cout << "Decision: " << result.action
                  << " (conf=" << result.confidence
                  << " rec=" << decision.recognitionScore
                  << " qual=" << decision.qualityScore
                  << " rule=" << decision.ruleScore
                  << " corr=" << decision.correctionScore
                  << " anom=" << decision.anomalyScore << ")\n";

        // ================================================================
        // Stage 6: Timing
        // ================================================================
        auto end = std::chrono::steady_clock::now();
        result.processTimeMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        std::cout << "Process time: " << result.processTimeMs << " ms\n";

        // ================================================================
        // Stage 7: Alarm
        // ================================================================
        std::cout << "Alarm: ";
        alarm.apply(result.riskLevel);

        // ================================================================
        // Stage 8: Log & Stats + Hash (V5)
        // ================================================================
        logger.write(result);
        stats.update(result);
        HashRecord hrec = hashLogger.buildRecord(frameIdx, result);
        hashLogger.append(hrec);

        // Track recent high-risk events for HUD
        if (result.riskLevel >= RiskLevel::LEVEL_3_HIGH) {
            std::string ev = result.imageName + ": "
                           + std::string(riskEngine.levelName(result.riskLevel))
                           + " - " + result.message;
            stats.addRecentEvent(ev);
        }

        // ================================================================
        // Stage 9: Display
        // ================================================================
        cv::Mat display;
        image.copyTo(display);
        drawHUD(display, result, stats, riskEngine, frameIdx, totalFrames);

        // Save annotated frame
        std::filesystem::create_directories("../output");
        cv::imwrite("../output/frame_" + std::to_string(frameIdx) + ".png", display);

        if (!headless) {
            cv::namedWindow("i.MX93 Sorting Simulator", cv::WINDOW_NORMAL);
            cv::resizeWindow("i.MX93 Sorting Simulator", 960, 640);
            cv::imshow("i.MX93 Sorting Simulator", display);

            int key = cv::waitKey(waitMs) & 0xFF;
            if (key == 'q' || key == 27) {
                std::cout << "\n[USER] Quit requested.\n";
                break;
            }
        } else {
            std::cout << "[Headless] Saved ../output/frame_" << frameIdx << ".png\n";
        }
    }

    // ---- Final Summary ----
    alarm.reset();
    stats.printSummary();

    // V5: Hash chain audit
    std::cout << "\n--- Hash Chain Audit (V5) ---\n";
    bool chainOk = hashLogger.verifyAll();
    std::cout << "Hash chain verification: " << (chainOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Chain length: " << hashLogger.records().size() << " events\n";
    hashLogger.writeCSV("../logs/hash_chain.csv");
    std::cout << "[DONE] Hash chain saved to ../logs/hash_chain.csv\n";

    if (!headless) cv::destroyAllWindows();
    std::cout << "\n[DONE] Log saved to ../logs/events.csv\n";
    std::cout << "[DONE] Run validation with: python validate.py ../logs/events.csv expected.csv\n";
    return 0;
}
