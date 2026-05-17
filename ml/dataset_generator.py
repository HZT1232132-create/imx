"""
Synthetic PKGxxx dataset generator for Tiny-ID-OCR training.

Characters: P K G 0-9 O I L
Degradations: blur, noise, glare, rotation, perspective, occlusion, low_contrast

Output structure:
  dataset_id_ocr/
    train/images/ + labels.txt
    val/images/ + labels.txt
    test_degraded/<type>/images/ + labels.txt
"""
import os, sys, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

CHARSET = "PKG0123456789OIL"
FONT_PATHS = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
]
PKG_PREFIX = "PKG"


def find_font():
    for p in FONT_PATHS:
        if os.path.exists(p):
            return p
    # fallback: search
    for root, dirs, files in os.walk("/usr/share/fonts"):
        for f in files:
            if f.endswith(".ttf") and "DejaVu" in f:
                return os.path.join(root, f)
    return None


def random_id():
    """Generate a random PKGxxx ID, with deliberate OCR-confusable chars."""
    suffix_len = random.randint(3, 5)
    normal_chars = "0123456789"
    confuse_chars = "OIL"  # OCR confusable
    suffix = ""
    for _ in range(suffix_len):
        if random.random() < 0.25:
            suffix += random.choice(confuse_chars)
        else:
            suffix += random.choice(normal_chars)
    return PKG_PREFIX + suffix


def render_text(text, font_path, font_size=32):
    """Render white text on black background."""
    font = ImageFont.truetype(font_path, font_size)
    # measure text
    dummy = Image.new("L", (1, 1))
    draw = ImageDraw.Draw(dummy)
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]

    img = Image.new("L", (tw + 20, th + 16), 0)
    draw = ImageDraw.Draw(img)
    x = random.randint(4, 12)
    y = random.randint(4, 8)
    draw.text((x, y), text, fill=255, font=font)
    return np.array(img)


def apply_noise(img_np, severity=0.5):
    noise = np.random.normal(0, 25 * severity, img_np.shape).astype(np.int16)
    noisy = img_np.astype(np.int16) + noise
    return np.clip(noisy, 0, 255).astype(np.uint8)


def apply_blur(img_np, severity=0.5):
    ksize = max(1, int(3 + severity * 6))
    if ksize % 2 == 0:
        ksize += 1
    img = Image.fromarray(img_np)
    img = img.filter(ImageFilter.GaussianBlur(radius=severity * 3))
    return np.array(img)


def apply_glare(img_np, severity=0.5):
    h, w = img_np.shape
    result = img_np.copy().astype(np.float32)
    num_spots = random.randint(1, int(3 + severity * 5))
    for _ in range(num_spots):
        cx = random.randint(0, w - 1)
        cy = random.randint(0, h - 1)
        rx = random.randint(5, max(5, int(w * 0.3)))
        ry = random.randint(3, max(3, int(h * 0.3)))
        strength = random.uniform(0.3, 0.9) * severity
        Y, X = np.ogrid[:h, :w]
        dist = ((X - cx) ** 2 / rx ** 2 + (Y - cy) ** 2 / ry ** 2)
        mask = np.exp(-dist * 2) * strength * 200
        result += mask
    return np.clip(result, 0, 255).astype(np.uint8)


def apply_rotation(img_np, severity=0.5):
    angle = random.uniform(-20, 20) * severity
    img = Image.fromarray(img_np)
    img = img.rotate(angle, expand=True, fillcolor=0)
    return np.array(img)


def apply_perspective(img_np, severity=0.5):
    h, w = img_np.shape
    src = np.float32([[0, 0], [w, 0], [0, h], [w, h]])
    shift = int(severity * h * 0.25)
    dst = np.float32([
        [random.randint(0, shift), random.randint(0, shift)],
        [w - random.randint(0, shift), random.randint(0, shift)],
        [random.randint(0, shift), h - random.randint(0, shift)],
        [w - random.randint(0, shift), h - random.randint(0, shift)],
    ])
    from PIL import Image as PILImage
    M = cv2_get_perspective(src, dst)
    img = Image.fromarray(img_np)
    img = img.transform((w, h), Image.PERSPECTIVE,
                        (M[0, 0], M[0, 1], M[0, 2],
                         M[1, 0], M[1, 1], M[1, 2],
                         M[2, 0], M[2, 1]),
                        fillcolor=0)
    return np.array(img)


def cv2_get_perspective(src, dst):
    A = []
    for (x, y), (u, v) in zip(src, dst):
        A.append([x, y, 1, 0, 0, 0, -u * x, -u * y])
        A.append([0, 0, 0, x, y, 1, -v * x, -v * y])
    A = np.array(A, dtype=np.float64)
    B = dst.reshape(-1)
    try:
        h = np.linalg.solve(A, B)
    except np.linalg.LinAlgError:
        return np.eye(3, dtype=np.float32)
    h = np.append(h, 1.0).reshape(3, 3)
    return h.astype(np.float32)


def apply_occlusion(img_np, severity=0.5):
    h, w = img_np.shape
    result = img_np.copy()
    num_blocks = random.randint(1, int(2 + severity * 4))
    for _ in range(num_blocks):
        bw = random.randint(5, max(5, int(w * 0.25)))
        bh = random.randint(5, max(5, int(h * 0.25)))
        bx = random.randint(0, w - bw)
        by = random.randint(0, h - bh)
        fill_val = random.randint(0, 60)
        result[by:by + bh, bx:bx + bw] = fill_val
    return result


def apply_low_contrast(img_np, severity=0.5):
    vmin, vmax = np.percentile(img_np, [5, 95])
    if vmax - vmin < 10:
        return img_np
    scale = 1.0 - severity * 0.7
    mean = (vmin + vmax) / 2.0
    result = (img_np.astype(np.float32) - mean) * scale + mean
    return np.clip(result, 0, 255).astype(np.uint8)


DEGRADATIONS = {
    "noise": apply_noise,
    "blur": apply_blur,
    "glare": apply_glare,
    "rotation": apply_rotation,
    "perspective": apply_perspective,
    "occlusion": apply_occlusion,
    "low_contrast": apply_low_contrast,
}


def generate_sample(font_path, text, degradation=None, severity=0.5):
    font_size = random.randint(24, 40)
    img_np = render_text(text, font_path, font_size)
    if degradation and degradation in DEGRADATIONS:
        img_np = DEGRADATIONS[degradation](img_np, severity)
    # Resize to fixed dimensions (32 x variable width)
    h, w = img_np.shape
    target_h = 32
    if h != target_h:
        scale = target_h / h
        new_w = max(target_h, int(w * scale))
        img = Image.fromarray(img_np)
        img = img.resize((new_w, target_h), Image.LANCZOS)
        img_np = np.array(img)
    return img_np


def generate_dataset(output_dir, train_count=5000, val_count=1000):
    font_path = find_font()
    if not font_path:
        print("ERROR: No font found!")
        sys.exit(1)
    print(f"Using font: {font_path}")

    os.makedirs(f"{output_dir}/train/images", exist_ok=True)
    os.makedirs(f"{output_dir}/val/images", exist_ok=True)

    # ---- Train set ----
    print(f"\nGenerating {train_count} training samples...")
    with open(f"{output_dir}/train/labels.txt", "w") as lbl:
        for i in range(train_count):
            text = random_id()
            # mix: 50% clean, 50% degraded (random type)
            if random.random() < 0.5:
                deg_type = random.choice(list(DEGRADATIONS.keys()))
                severity = random.uniform(0.2, 1.0)
            else:
                deg_type = None
                severity = 0
            img_np = generate_sample(font_path, text, deg_type, severity)
            fname = f"train_{i:05d}.png"
            Image.fromarray(img_np).save(f"{output_dir}/train/images/{fname}")
            lbl.write(f"images/{fname}\t{text}\n")
            if (i + 1) % 1000 == 0:
                print(f"  {i + 1}/{train_count}")

    # ---- Validation set (clean only) ----
    print(f"\nGenerating {val_count} validation samples...")
    with open(f"{output_dir}/val/labels.txt", "w") as lbl:
        for i in range(val_count):
            text = random_id()
            img_np = generate_sample(font_path, text, None, 0)
            fname = f"val_{i:05d}.png"
            Image.fromarray(img_np).save(f"{output_dir}/val/images/{fname}")
            lbl.write(f"images/{fname}\t{text}\n")
            if (i + 1) % 1000 == 0:
                print(f"  {i + 1}/{val_count}")

    # ---- Test degraded sets ----
    print(f"\nGenerating degradation test sets...")
    for deg_name in DEGRADATIONS:
        test_dir = f"{output_dir}/test_degraded/{deg_name}"
        os.makedirs(f"{test_dir}/images", exist_ok=True)
        count = 200
        with open(f"{test_dir}/labels.txt", "w") as lbl:
            for i in range(count):
                text = random_id()
                severity = random.choice([0.3, 0.5, 0.7, 0.9])
                img_np = generate_sample(font_path, text, deg_name, severity)
                fname = f"{deg_name}_{i:05d}.png"
                Image.fromarray(img_np).save(f"{test_dir}/images/{fname}")
                lbl.write(f"images/{fname}\t{text}\n")
        print(f"  {deg_name}: {count} samples")

    print(f"\nDataset generated at: {output_dir}")


if __name__ == "__main__":
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "../dataset_id_ocr"
    train_n = int(sys.argv[2]) if len(sys.argv) > 2 else 5000
    val_n = int(sys.argv[3]) if len(sys.argv) > 3 else 1000
    generate_dataset(out_dir, train_n, val_n)
