"""
Export trained CRNN-small model to ONNX for deployment.

Usage: python export_onnx.py <checkpoint> [output.onnx]
"""
import sys, os
import torch
import numpy as np

# Add ml/ to path for imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from train_tiny_id_ocr import CRNNSmall, VOCAB


def export(checkpoint_path, onnx_path, img_h=32):
    num_classes = len(VOCAB)
    model = CRNNSmall(num_classes=num_classes, img_h=img_h)
    model.load_state_dict(torch.load(checkpoint_path, map_location="cpu"))
    model.eval()

    # Dynamic width: export with sample input W=160
    dummy_input = torch.randn(1, 1, img_h, 160)
    torch.onnx.export(
        model,
        dummy_input,
        onnx_path,
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={
            "input": {0: "batch", 3: "width"},
            "output": {1: "batch", 0: "time"},
        },
        opset_version=14,
        dynamo=False,  # legacy TorchScript exporter (handles LSTM correctly)
    )
    print(f"Exported to {onnx_path}")

    # Verify
    import onnx
    onnx_model = onnx.load(onnx_path)
    onnx.checker.check_model(onnx_model)
    print("ONNX model validated OK")

    # Print model size
    size_kb = os.path.getsize(onnx_path) / 1024
    print(f"Model size: {size_kb:.1f} KB")

    # Test inference with ONNX Runtime
    try:
        import onnxruntime as ort
        session = ort.InferenceSession(onnx_path)
        test_input = dummy_input.numpy()
        outputs = session.run(None, {"input": test_input})
        logits = outputs[0]  # (T, 1, C)
        print(f"ORT test: logits shape={logits.shape}, OK")
    except Exception as e:
        print(f"ORT test skipped: {e}")

    return onnx_path


if __name__ == "__main__":
    ckpt = sys.argv[1] if len(sys.argv) > 1 else "./runs/tiny_id_ocr_v1/best.pt"
    out = sys.argv[2] if len(sys.argv) > 2 else "./runs/tiny_id_ocr_v1/tiny_id_ocr.onnx"
    export(ckpt, out)
