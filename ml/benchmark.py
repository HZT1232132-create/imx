"""
V4 benchmark: accuracy per degradation type + latency measurement.

Outputs: benchmark_report.csv
"""
import os, sys, time, csv
import numpy as np
from PIL import Image
import onnxruntime as ort

CHARSET = "PKG0123456789OIL"


def ctc_decode(logits):
    """CTC greedy decode from (T, C) logits."""
    pred = np.argmax(logits, axis=1)
    decoded = []
    prev = 0
    for idx in pred:
        if idx != 0 and idx != prev and idx <= len(CHARSET):
            decoded.append(CHARSET[idx - 1])
        prev = idx
    return "".join(decoded)


def load_labels(labels_path):
    samples = []
    base_dir = os.path.dirname(labels_path)
    with open(labels_path, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) >= 2:
                samples.append((os.path.join(base_dir, parts[0]), parts[1]))
    return samples


def preprocess(img_path, fixed_w=None):
    img = Image.open(img_path).convert("L")
    h, w = img.height, img.width
    scale = 32.0 / h
    new_w = max(32, int(w * scale))
    img = img.resize((new_w, 32), Image.LANCZOS)
    arr = np.array(img, dtype=np.float32) / 255.0
    arr = arr[np.newaxis, np.newaxis, :, :]
    if fixed_w:
        # pad or resize to fixed width
        cur_w = arr.shape[3]
        if cur_w < fixed_w:
            pad = np.zeros((1, 1, 32, fixed_w - cur_w), dtype=np.float32)
            arr = np.concatenate([arr, pad], axis=3)
        elif cur_w > fixed_w:
            img = Image.fromarray((arr[0, 0] * 255).astype(np.uint8))
            img = img.resize((fixed_w, 32), Image.LANCZOS)
            arr = np.array(img, dtype=np.float32)[np.newaxis, np.newaxis, :, :] / 255.0
    return arr


def run_benchmark(onnx_path, dataset_dir, output_csv):
    print(f"Loading ONNX model: {onnx_path}")
    session = ort.InferenceSession(onnx_path)

    # Warmup
    dummy = np.random.randn(1, 1, 32, 160).astype(np.float32)
    for _ in range(5):
        session.run(None, {"input": dummy})

    results = []

    # ---- 1. Clean validation accuracy ----
    print("\n--- Clean validation ---")
    val_samples = load_labels(f"{dataset_dir}/val/labels.txt")
    correct = 0
    total_time = 0
    for img_path, gt in val_samples[:500]:
        arr = preprocess(img_path)
        t0 = time.perf_counter()
        out = session.run(None, {"input": arr})[0]  # (T, 1, C)
        t1 = time.perf_counter()
        total_time += (t1 - t0) * 1000
        pred = ctc_decode(out[:, 0, :])
        if pred == gt:
            correct += 1
    acc = 100.0 * correct / 500
    avg_ms = total_time / 500
    print(f"  Accuracy: {acc:.1f}%, Latency: {avg_ms:.2f}ms")
    results.append(["clean", acc, avg_ms])

    # ---- 2. Per-degradation accuracy ----
    degradations = ["blur", "noise", "glare", "rotation", "perspective", "occlusion", "low_contrast"]
    for deg in degradations:
        deg_dir = f"{dataset_dir}/test_degraded/{deg}"
        labels_path = f"{deg_dir}/labels.txt"
        if not os.path.exists(labels_path):
            print(f"  {deg}: SKIP (no data)")
            results.append([deg, 0, 0])
            continue

        samples = load_labels(labels_path)
        correct = 0
        total_time = 0
        for img_path, gt in samples:
            arr = preprocess(img_path)
            t0 = time.perf_counter()
            out = session.run(None, {"input": arr})[0]
            t1 = time.perf_counter()
            total_time += (t1 - t0) * 1000
            pred = ctc_decode(out[:, 0, :])
            if pred == gt:
                correct += 1
        acc = 100.0 * correct / len(samples)
        avg_ms = total_time / len(samples) if samples else 0
        print(f"  {deg}: Accuracy={acc:.1f}%, Latency={avg_ms:.2f}ms")
        results.append([deg, acc, avg_ms])

    # ---- 3. Model sizes ----
    onnx_kb = os.path.getsize(onnx_path) / 1024
    tflite_path = onnx_path.replace(".onnx", "_int8.tflite").replace("runs/tiny_id_ocr_v1/", "models/")
    if not os.path.exists(tflite_path):
        tflite_path = "models/tiny_id_ocr_int8.tflite"
    tflite_kb = os.path.getsize(tflite_path) / 1024 if os.path.exists(tflite_path) else 0
    pt_kb = 5084  # approximate

    print(f"\n--- Model Size ---")
    print(f"  PyTorch .pt:  {pt_kb:.0f} KB")
    print(f"  ONNX:         {onnx_kb:.0f} KB")
    print(f"  TFLite FP16:  {tflite_kb:.0f} KB")

    # ---- Write report ----
    os.makedirs(os.path.dirname(output_csv) or ".", exist_ok=True)
    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["test_type", "accuracy_pct", "latency_ms"])
        for row in results:
            writer.writerow([row[0], f"{row[1]:.1f}", f"{row[2]:.2f}"])
        writer.writerow([])
        writer.writerow(["model", "size_kb", "notes"])
        writer.writerow(["PyTorch .pt", f"{pt_kb:.0f}", "training checkpoint"])
        writer.writerow(["ONNX", f"{onnx_kb:.0f}", "C++ inference (CPU)"])
        writer.writerow(["TFLite FP16", f"{tflite_kb:.0f}", "NPU path (Vela compile pending)"])
        writer.writerow(["i.MX93 NPU", "N/A", "requires FRDM-i.MX93 + eIQ Vela"])

    print(f"\nReport saved: {output_csv}")
    return results


if __name__ == "__main__":
    onnx_path = sys.argv[1] if len(sys.argv) > 1 else "models/tiny_id_ocr.onnx"
    dataset_dir = sys.argv[2] if len(sys.argv) > 2 else "dataset_id_ocr"
    output_csv = sys.argv[3] if len(sys.argv) > 3 else "bench/benchmark_report.csv"
    run_benchmark(onnx_path, dataset_dir, output_csv)
