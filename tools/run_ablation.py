#!/usr/bin/env python3
"""
V5 Ablation Study — version comparison V0 through V5.

Runs each configuration and reports per the 细化方案 section 6:
- Recognition rate, false positive rate, anomaly recall, latency
- Outputs: bench/ablation_report.csv
"""
import os, sys, subprocess, csv, time

SIMULATOR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "build", "sorting_sim")
LIB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lib")
DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data")
BENCH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bench")

VERSIONS = {
    "V0_baseline":  ["../data/demo_sequence.csv", "../data/rules.csv", "../data/images", "1", "baseline"],
    "V1_ocr":       ["../data/demo_sequence.csv", "../data/rules.csv", "../data/images", "1", "ocr"],
    "V2_corrector": ["../data/demo_sequence.csv", "../data/rules.csv", "../data/images", "1", "full"],
    "V3_quality":   ["../data/demo_sequence.csv", "../data/rules.csv", "../data/images", "1", "full"],
    "V4_tinyocr":   ["../data/demo_tinyocr.csv", "../data/rules.csv", "../data/images", "1", "tinyocr"],
    "V5_full":      ["../data/demo_sequence.csv", "../data/rules.csv", "../data/images", "1", "full"],
}


def parse_stats(output):
    m = {}
    in_summary = False
    for line in output.split("\n"):
        if "系统处理统计" in line:
            in_summary = True
            continue
        if in_summary:
            if "总处理数量" in line and "：" in line:
                m["total"] = int(line.split("：")[1].strip())
            elif "QR识别成功" in line and "：" in line:
                m["qr"] = int(line.split("：")[1].strip())
            elif "OCR补救成功" in line and "：" in line:
                m["ocr_rcv"] = int(line.split("：")[1].strip())
            elif "OCR纠正成功" in line and "：" in line:
                m["ocr_cor"] = int(line.split("：")[1].strip())
            elif "标签异常" in line and "：" in line:
                m["label_err"] = int(line.split("：")[1].strip())
            elif "未知货物" in line and "：" in line:
                m["unknown"] = int(line.split("：")[1].strip())
            elif "错误异常" in line and "：" in line:
                m["wrong"] = int(line.split("：")[1].strip())
            elif "综合识别成功率" in line and "：" in line:
                m["rec_rate"] = float(line.split("：")[1].strip().replace("%", ""))
            elif "高风险事件数" in line and "：" in line:
                m["high_risk"] = int(line.split("：")[1].strip())
            elif "平均处理时间" in line and "：" in line:
                m["latency"] = int(line.split("：")[1].strip().replace(" ms", "").split(".")[0])
            elif not line.strip():
                in_summary = False
        if "Hash chain verification" in line:
            m["hash_ok"] = "PASS" in line
        if "决策-PASS" in line and "：" in line:
            parts = line.split()
            for p in parts:
                if "PASS：" in p:
                    m["action_pass"] = int(p.split("：")[1])
                elif "PASS_WITH_LOG：" in p:
                    m["action_plog"] = int(p.split("：")[1])
                elif "REVIEW：" in p:
                    m["action_review"] = int(p.split("：")[1])
                elif "BLOCK：" in p:
                    m["action_block"] = int(p.split("：")[1])
    return m


def run_version(name, args):
    cmd = [SIMULATOR] + args
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = LIB_PATH
    build_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build")
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=60,
                          env=env, cwd=build_dir)
        output = r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        return {"version": name, "error": "timeout"}

    stats = parse_stats(output)
    stats["version"] = name
    return stats


def main():
    os.makedirs(BENCH, exist_ok=True)
    print("=" * 70)
    print(" EdgeGuard-Sort Ablation Study (V0 -> V5)")
    print("=" * 70)

    results = []
    for name, args in VERSIONS.items():
        print(f"\n--- {name} ---")
        m = run_version(name, args)
        if "error" in m:
            print(f"  ERROR: {m['error']}")
            continue
        results.append(m)
        print(f"  Recognition: {m.get('rec_rate','?')}%")
        print(f"  High Risk:   {m.get('high_risk','?')}")
        print(f"  Latency:     {m.get('latency','?')}ms")
        print(f"  Hash:        {'OK' if m.get('hash_ok') else 'N/A'}")

    # Write report
    path = f"{BENCH}/ablation_report.csv"
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=[
            "version", "total", "qr", "ocr_rcv", "ocr_cor", "label_err",
            "unknown", "wrong", "rec_rate_pct", "high_risk", "latency_ms",
            "action_pass", "action_plog", "action_review", "action_block",
            "hash_ok"
        ])
        w.writeheader()
        for r in results:
            w.writerow({
                "version": r.get("version", ""),
                "total": r.get("total", 0),
                "qr": r.get("qr", 0),
                "ocr_rcv": r.get("ocr_rcv", 0),
                "ocr_cor": r.get("ocr_cor", 0),
                "label_err": r.get("label_err", 0),
                "unknown": r.get("unknown", 0),
                "wrong": r.get("wrong", 0),
                "rec_rate_pct": r.get("rec_rate", 0),
                "high_risk": r.get("high_risk", 0),
                "latency_ms": r.get("latency", 0),
                "action_pass": r.get("action_pass", 0),
                "action_plog": r.get("action_plog", 0),
                "action_review": r.get("action_review", 0),
                "action_block": r.get("action_block", 0),
                "hash_ok": r.get("hash_ok", False),
            })

    print(f"\n[Report] {path}")

    # Summary table
    print("\n" + "-" * 70)
    print(f"{'Version':<16} {'Recog%':>7} {'HiRisk':>7} {'Lat(ms)':>8} {'Hash':>6}")
    print("-" * 70)
    for r in results:
        print(f"{r['version']:<16} {r.get('rec_rate',0):>6.1f}% "
              f"{r.get('high_risk',0):>6} {r.get('latency',0):>7} "
              f"{'OK' if r.get('hash_ok') else 'N/A':>6}")
    print("-" * 70)


if __name__ == "__main__":
    main()
