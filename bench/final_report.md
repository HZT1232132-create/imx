# EdgeGuard-Sort V5 Final Report
Generated: 2026-05-11 20:41:04

## 1. Hash Chain Integrity
Status: **PASS**
```
============================================================
 Hash Chain Verification
============================================================
Chain: /mnt/c/Users/Administrator/Desktop/nxp竞赛/imx93_sorting_sim/tools/../logs/hash_chain.csv
Events: 6
Result: PASS

All events verified. Hash chain is intact.
```

## 2. Model Accuracy (Tiny-ID-OCR)
| Degradation | Accuracy | Latency |
|------------|----------|---------|
| clean | 100.0% | 1.25ms |
| blur | 88.5% | 1.30ms |
| noise | 100.0% | 1.29ms |
| glare | 99.0% | 1.44ms |
| rotation | 68.0% | 0.98ms |
| perspective | 85.0% | 1.30ms |
| occlusion | 78.5% | 1.37ms |
| low_contrast | 99.5% | 1.37ms |
| model | size_kb% | notesms |
| PyTorch .pt | 5084% | training checkpointms |
| ONNX | 5076% | C++ inference (CPU)ms |
| TFLite FP16 | 89% | NPU path (Vela compile pending)ms |
| i.MX93 NPU | N/A% | requires FRDM-i.MX93 + eIQ Velams |

## 3. Ablation Study (V0 → V5)
| Version | Recog% | HighRisk | Latency | Hash |
|---------|--------|----------|---------|------|
| V0_baseline | 33.3% | 5 | 24ms | OK |
| V1_ocr | 50.0% | 4 | 56ms | OK |
| V2_corrector | 66.7% | 3 | 44ms | OK |
| V3_quality | 66.7% | 3 | 48ms | OK |
| V4_tinyocr | 66.7% | 4 | 37ms | OK |
| V5_full | 66.7% | 3 | 43ms | OK |

## 4. Cross-Mode Robustness
| Mode | Recog% | HighRisk | Latency |
|------|--------|----------|---------|
| baseline | 33.3% | 5 | 26ms |
| ocr | 50.0% | 4 | 54ms |
| full | 66.7% | 3 | 48ms |
| tinyocr | 33.3% | 5 | 43ms |

## 5. Model Size Comparison
| Format | Size |
|--------|------|
| PyTorch .pt | 5084 KB |
| ONNX | 5075 KB |
| TFLite FP16 | 88 KB |
