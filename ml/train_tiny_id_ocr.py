"""
Train CRNN-small model for Tiny-ID-OCR (PKGxxx format recognition).

Architecture: Conv blocks → BiLSTM → Linear (CTC)
Charset: PKG0123456789OIL + CTC blank
Input: 1x32xW grayscale, Output: sequence of character logits
"""
import os, sys, random
import numpy as np
from PIL import Image
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader

CHARSET = "PKG0123456789OIL"
BLANK_IDX = 0  # CTC blank is index 0
VOCAB = ["<BLANK>"] + list(CHARSET)  # blank at 0, then chars
CHAR2IDX = {c: i for i, c in enumerate(VOCAB)}
IDX2CHAR = {i: c for i, c in enumerate(VOCAB)}


class PKGDataset(Dataset):
    def __init__(self, labels_path, img_h=32):
        self.samples = []
        self.img_h = img_h
        base_dir = os.path.dirname(labels_path)
        with open(labels_path, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = line.split("\t")
                if len(parts) >= 2:
                    img_rel = parts[0]
                    label = parts[1]
                    self.samples.append((os.path.join(base_dir, img_rel), label))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        img_path, label = self.samples[idx]
        img = Image.open(img_path).convert("L")
        img_np = np.array(img, dtype=np.float32) / 255.0
        h, w = img_np.shape
        # Resize height to img_h, keep aspect ratio
        scale = self.img_h / h
        new_w = max(self.img_h, int(w * scale))
        img = Image.fromarray((img_np * 255).astype(np.uint8))
        img = img.resize((new_w, self.img_h), Image.LANCZOS)
        img_np = np.array(img, dtype=np.float32) / 255.0
        img_tensor = torch.tensor(img_np).unsqueeze(0)  # (1, H, W)

        # Label to indices (no blank between chars for training — CTC handles it)
        label_indices = [CHAR2IDX.get(c, 0) for c in label]
        return img_tensor, torch.tensor(label_indices, dtype=torch.long)


def collate_batch(batch):
    images, labels = zip(*batch)
    # Pad images to max width
    max_w = max(img.shape[2] for img in images)
    padded = []
    for img in images:
        w = img.shape[2]
        if w < max_w:
            pad = torch.zeros(1, img.shape[1], max_w - w)
            img = torch.cat([img, pad], dim=2)
        padded.append(img)
    images = torch.stack(padded)  # (B, 1, H, W)

    # Concatenate labels for CTC (requires flat tensor)
    label_lengths = torch.tensor([len(l) for l in labels], dtype=torch.long)
    labels_flat = torch.cat([l for l in labels])

    return images, labels_flat, label_lengths


class CRNNSmall(nn.Module):
    def __init__(self, num_classes, img_h=32):
        super().__init__()
        self.img_h = img_h

        # CNN backbone — light enough for i.MX93
        self.cnn = nn.Sequential(
            nn.Conv2d(1, 32, 3, padding=1), nn.BatchNorm2d(32), nn.ReLU(),
            nn.MaxPool2d(2, 2),  # 32xW → 16xW/2

            nn.Conv2d(32, 64, 3, padding=1), nn.BatchNorm2d(64), nn.ReLU(),
            nn.MaxPool2d(2, 2),  # 16xW/2 → 8xW/4

            nn.Conv2d(64, 128, 3, padding=1), nn.BatchNorm2d(128), nn.ReLU(),
            nn.MaxPool2d((2, 1)),  # 8xW/4 → 4xW/4

            nn.Conv2d(128, 128, 3, padding=1), nn.BatchNorm2d(128), nn.ReLU(),
            # 4xW/4
        )
        self.cnn_output_channels = 128
        self.cnn_output_height = img_h // 8  # 32→16→8→4

        self.rnn = nn.LSTM(
            input_size=128 * self.cnn_output_height,
            hidden_size=128,
            num_layers=2,
            bidirectional=True,
            batch_first=True,
        )
        self.fc = nn.Linear(256, num_classes)  # 128*2 (bidirectional)

    def forward(self, x):
        # x: (B, 1, H, W)
        features = self.cnn(x)  # (B, 128, 4, W')
        B, C, H, W = features.shape
        features = features.permute(0, 3, 1, 2)  # (B, W', 128, H)
        features = features.reshape(B, W, C * H)  # (B, W', 512)
        rnn_out, _ = self.rnn(features)  # (B, W', 256)
        logits = self.fc(rnn_out)  # (B, W', num_classes)
        return logits.log_softmax(2).permute(1, 0, 2)  # (T, B, C) for CTC


def decode(logits, blank=0):
    """Greedy CTC decode: logits (T, B, C) → list of strings."""
    _, max_indices = logits.max(2)  # (T, B)
    max_indices = max_indices.cpu().numpy()
    results = []
    for b in range(max_indices.shape[1]):
        seq = max_indices[:, b]
        decoded = []
        prev = blank
        for idx in seq:
            if idx != blank and idx != prev:
                decoded.append(IDX2CHAR.get(idx, ""))
            prev = idx
        results.append("".join(decoded))
    return results


def train(model, train_loader, val_loader, epochs, device, out_dir):
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, epochs)
    ctc_loss = nn.CTCLoss(blank=BLANK_IDX, zero_infinity=True)

    best_acc = 0
    for epoch in range(epochs):
        model.train()
        total_loss = 0
        for batch_idx, (images, labels_flat, label_lengths) in enumerate(train_loader):
            images = images.to(device)
            labels_flat = labels_flat.to(device)
            label_lengths = label_lengths.to(device)

            logits = model(images)  # (T, B, C)
            T = logits.shape[0]
            B = images.shape[0]
            input_lengths = torch.full((B,), T, dtype=torch.long, device=device)

            loss = ctc_loss(logits, labels_flat, input_lengths, label_lengths)
            optimizer.zero_grad()
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), 5.0)
            optimizer.step()
            total_loss += loss.item()

            if (batch_idx + 1) % 50 == 0:
                print(f"  Epoch {epoch+1}/{epochs}, batch {batch_idx+1}, loss={loss.item():.4f}")

        scheduler.step()
        avg_loss = total_loss / max(1, len(train_loader))

        # Validation accuracy
        acc = evaluate(model, val_loader, device)
        print(f"Epoch {epoch+1}/{epochs}: train_loss={avg_loss:.4f}, val_acc={acc:.2%}")

        if acc > best_acc:
            best_acc = acc
            torch.save(model.state_dict(), f"{out_dir}/best.pt")
            print(f"  => Best model saved (acc={best_acc:.2%})")

    print(f"\nTraining done. Best val accuracy: {best_acc:.2%}")


def evaluate(model, loader, device):
    model.eval()
    correct = 0
    total = 0
    with torch.no_grad():
        for images, labels_flat, label_lengths in loader:
            images = images.to(device)
            logits = model(images)
            preds = decode(logits)
            # Reconstruct ground truth labels
            offset = 0
            for b, length in enumerate(label_lengths):
                gt = "".join(IDX2CHAR.get(labels_flat[offset + i].item(), "")
                             for i in range(length.item()))
                offset += length.item()
                if preds[b] == gt:
                    correct += 1
                total += 1
    return correct / max(1, total)


if __name__ == "__main__":
    data_dir = sys.argv[1] if len(sys.argv) > 1 else "../dataset_id_ocr"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "./runs/tiny_id_ocr_v1"
    epochs = int(sys.argv[3]) if len(sys.argv) > 3 else 50
    batch_size = int(sys.argv[4]) if len(sys.argv) > 4 else 64

    os.makedirs(out_dir, exist_ok=True)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Device: {device}")

    train_ds = PKGDataset(f"{data_dir}/train/labels.txt")
    val_ds = PKGDataset(f"{data_dir}/val/labels.txt")
    print(f"Train: {len(train_ds)}, Val: {len(val_ds)}")

    train_loader = DataLoader(train_ds, batch_size=batch_size, shuffle=True,
                              collate_fn=collate_batch, num_workers=0)
    val_loader = DataLoader(val_ds, batch_size=batch_size, shuffle=False,
                            collate_fn=collate_batch, num_workers=0)

    model = CRNNSmall(num_classes=len(VOCAB)).to(device)
    num_params = sum(p.numel() for p in model.parameters())
    print(f"Model params: {num_params:,} ({num_params/1e6:.2f}M)")

    train(model, train_loader, val_loader, epochs, device, out_dir)
