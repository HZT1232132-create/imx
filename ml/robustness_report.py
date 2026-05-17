"""
V5 Robustness Report: cross-mode comparison.

Runs all modes (baseline/ocr/full/tinyocr) and compares:
- Recognition rate, false positive rate, latency, high-risk events
- Outputs: bench/result_summary.csv
"""
import os, sys, subprocess, csv, time

SIMULATOR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "build", "sorting_sim")
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data")
OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bench")

MODES = ["baseline", "ocr", "full", "tinyocr"]
SEQ = f"{DATA_DIR}/demo_sequence.csv"
RULES = f"{DATA_DIR}/rules.csv"
IMAGES = f"{DATA_DIR}/images"


def run_mode(mode):
    """Run simulator in given mode, return metrics."""
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                          "..", "lib")

    log_path = f"../logs/events.csv"  # Simulator always writes here

    cmd = [SIMULATOR, SEQ, RULES, IMAGES, "1", mode]
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=60, env=env,
            cwd=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build"),
        )
        output = result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        return {"mode": mode, "error": "timeout"}

    metrics = {"mode": mode}

    # Parse stats from output (only within the summary section)
    in_summary = False
    for line in output.split("\n"):
        if "系统处理统计" in line:
            in_summary = True
            continue
        if in_summary:
            if "总处理数量" in line:
                metrics["total"] = int(line.split("：")[-1].strip())
            elif "QR识别成功" in line:
                metrics["qr_success"] = int(line.split("：")[-1].strip())
            elif "OCR补救成功" in line:
                metrics["ocr_recovered"] = int(line.split("：")[-1].strip())
            elif "OCR纠正成功" in line:
                metrics["ocr_corrected"] = int(line.split("：")[-1].strip())
            elif "标签异常" in line and "：" in line:
                metrics["label_error"] = int(line.split("：")[-1].strip())
            elif "未知货物" in line:
                metrics["unknown"] = int(line.split("：")[-1].strip())
            elif "错误异常" in line:
                metrics["wrong_sort"] = int(line.split("：")[-1].strip())
            elif "综合识别成功率" in line:
                metrics["recognition_rate"] = line.split("：")[-1].strip().replace("%", "")
            elif "高风险事件数" in line:
                metrics["high_risk"] = int(line.split("：")[-1].strip())
            elif "平均处理时间" in line:
                metrics["avg_latency_ms"] = line.split("：")[-1].strip().replace(" ms", "")
            elif "QC" in line or "===" in line:
                continue
            elif not line.strip():
                in_summary = False
        if "Hash chain verification" in line:
            metrics["hash_ok"] = "PASS" in line

    return metrics


def generate_robustness_report():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print("=" * 60)
    print(" EdgeGuard-Sort V5 Robustness Report")
    print("=" * 60)

    all_metrics = []
    for mode in MODES:
        if mode == "tinyocr" and not os.path.exists(
            os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "models", "tiny_id_ocr.onnx")
        ):
            print(f"\n  {mode}: SKIP (model not found)")
            continue

        print(f"\n--- {mode} mode ---")
        metrics = run_mode(mode)
        if "error" in metrics:
            print(f"  ERROR: {metrics['error']}")
            continue

        all_metrics.append(metrics)
        rec = metrics.get("recognition_rate", "N/A")
        lat = metrics.get("avg_latency_ms", "N/A")
        hr = metrics.get("high_risk", "N/A")
        hok = metrics.get("hash_ok", False)
        print(f"  Recognition: {rec}%")
        print(f"  Avg latency: {lat}ms")
        print(f"  High risk events: {hr}")
        print(f"  Hash chain: {'OK' if hok else 'N/A'}")

    # Write result_summary.csv
    summary_path = f"{OUTPUT_DIR}/result_summary.csv"
    fieldnames = ["mode", "total", "qr_success", "ocr_recovered", "ocr_corrected",
                  "label_error", "unknown", "wrong_sort", "recognition_rate_pct",
                  "high_risk", "avg_latency_ms"]
    with open(summary_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        for m in all_metrics:
            writer.writerow({
                "mode": m.get("mode", ""),
                "total": m.get("total", 0),
                "qr_success": m.get("qr_success", 0),
                "ocr_recovered": m.get("ocr_recovered", 0),
                "ocr_corrected": m.get("ocr_corrected", 0),
                "label_error": m.get("label_error", 0),
                "unknown": m.get("unknown", 0),
                "wrong_sort": m.get("wrong_sort", 0),
                "recognition_rate_pct": m.get("recognition_rate", ""),
                "high_risk": m.get("high_risk", 0),
                "avg_latency_ms": m.get("avg_latency_ms", ""),
            })

    print(f"\n[Report] {summary_path}")

    # Print comparison table
    if all_metrics:
        print("\n" + "=" * 70)
        print(f"{'Mode':<12} {'Recog%':>7} {'Latency':>8} {'HighRisk':>9} {'HashOK':>7}")
        print("-" * 70)
        for m in all_metrics:
            print(f"{m['mode']:<12} {m.get('recognition_rate','?'):>6}% "
                  f"{m.get('avg_latency_ms','?'):>7}ms "
                  f"{m.get('high_risk','?'):>8} "
                  f"{'YES' if m.get('hash_ok') else 'N/A':>7}")
        print("=" * 70)


if __name__ == "__main__":
    generate_robustness_report()
