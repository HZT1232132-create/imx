#!/usr/bin/env python3
"""
V5 Final Report Generator.

Aggregates: benchmark, ablation, hash verification into a single markdown report.
"""
import os, sys, csv, subprocess
from datetime import datetime


def run_verify_log():
    """Run hash chain verification."""
    verifier = os.path.join(os.path.dirname(os.path.abspath(__file__)), "verify_log.py")
    log = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "logs", "hash_chain.csv")
    if not os.path.exists(log):
        return {"status": "SKIP", "reason": "no hash_chain.csv"}
    try:
        r = subprocess.run([sys.executable, verifier, log], capture_output=True, text=True, timeout=10)
        return {"status": "PASS" if r.returncode == 0 else "FAIL",
                "output": r.stdout.strip()}
    except Exception as e:
        return {"status": "ERROR", "reason": str(e)}


def read_csv(path):
    if not os.path.exists(path):
        return None
    with open(path, "r") as f:
        return list(csv.DictReader(f))


def generate():
    base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    report_path = os.path.join(base, "bench", "final_report.md")

    lines = []
    lines.append("# EdgeGuard-Sort V5 Final Report")
    lines.append(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append("")

    # 1. Hash Verification
    lines.append("## 1. Hash Chain Integrity")
    hash_result = run_verify_log()
    lines.append(f"Status: **{hash_result['status']}**")
    if hash_result.get("output"):
        lines.append("```")
        lines.append(hash_result["output"])
        lines.append("```")
    lines.append("")

    # 2. Benchmark
    lines.append("## 2. Model Accuracy (Tiny-ID-OCR)")
    bench = read_csv(os.path.join(base, "bench", "benchmark_report.csv"))
    if bench:
        lines.append("| Degradation | Accuracy | Latency |")
        lines.append("|------------|----------|---------|")
        for row in bench:
            if row.get("test_type") and row.get("accuracy_pct"):
                lines.append(f"| {row['test_type']} | {row['accuracy_pct']}% | {row.get('latency_ms','?')}ms |")
    lines.append("")

    # 3. Ablation
    lines.append("## 3. Ablation Study (V0 → V5)")
    ablation = read_csv(os.path.join(base, "bench", "ablation_report.csv"))
    if ablation:
        lines.append("| Version | Recog% | HighRisk | Latency | Hash |")
        lines.append("|---------|--------|----------|---------|------|")
        for row in ablation:
            lines.append(f"| {row['version']} | {row.get('rec_rate_pct','?')}% | "
                        f"{row.get('high_risk','?')} | {row.get('latency_ms','?')}ms | "
                        f"{'OK' if row.get('hash_ok')=='True' else 'N/A'} |")
    lines.append("")

    # 4. Robustness
    lines.append("## 4. Cross-Mode Robustness")
    rob = read_csv(os.path.join(base, "bench", "result_summary.csv"))
    if rob:
        lines.append("| Mode | Recog% | HighRisk | Latency |")
        lines.append("|------|--------|----------|---------|")
        for row in rob:
            lines.append(f"| {row['mode']} | {row.get('recognition_rate_pct','?')}% | "
                        f"{row.get('high_risk','?')} | {row.get('avg_latency_ms','?')}ms |")
    lines.append("")

    # 5. Model Size
    lines.append("## 5. Model Size Comparison")
    lines.append("| Format | Size |")
    lines.append("|--------|------|")
    for label, path in [("PyTorch .pt", "runs/tiny_id_ocr_v1/best.pt"),
                         ("ONNX", "models/tiny_id_ocr.onnx"),
                         ("TFLite FP16", "models/tiny_id_ocr_int8.tflite")]:
        full = os.path.join(base, path)
        if os.path.exists(full):
            kb = os.path.getsize(full) // 1024
            lines.append(f"| {label} | {kb} KB |")
    lines.append("")

    with open(report_path, "w") as f:
        f.write("\n".join(lines))
    print(f"Report: {report_path}")


if __name__ == "__main__":
    generate()
