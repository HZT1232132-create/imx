"""
V5 Synthetic Anomaly Label Generator.

Generates 10 scenario types x ~100 images each for robustness testing.
Matches the 细化方案 spec (section 8.1).

Output: data/synthetic/<type>/images/ + labels.csv
"""
import os, sys, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import cv2

FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
QR_PATTERNS = ["PKG001", "PKG002", "PKG003", "PKG004", "PKG005"]
RULES = {
    "PKG001": "A", "PKG002": "B", "PKG003": "A", "PKG004": "C", "PKG005": "B",
}


def make_label_image(text, w=400, h=200):
    """Create a simulated label: white bg, black text + QR block."""
    font = ImageFont.truetype(FONT_PATH, 28)
    img = Image.new("L", (w, h), 255)
    draw = ImageDraw.Draw(img)
    # Text
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text(((w - tw) // 2 - 40, (h - th) // 2), text, fill=0, font=font)
    # Simulated QR block (right side)
    qr_x, qr_y = w - 70, h // 2 - 25
    for i in range(6):
        for j in range(6):
            if (i + j) % 2 == 0:
                draw.rectangle(
                    [qr_x + j * 7, qr_y + i * 7, qr_x + j * 7 + 5, qr_y + i * 7 + 5],
                    fill=0,
                )
    # Small label bar at top
    draw.rectangle([0, 0, w, 24], fill=200)
    draw.text((10, 2), "WAREHOUSE LABEL", fill=0, font=ImageFont.truetype(FONT_PATH, 14))
    return np.array(img)


def add_blur(img_np, severity):
    k = max(1, int(3 + severity * 10))
    if k % 2 == 0: k += 1
    return np.array(Image.fromarray(img_np).filter(ImageFilter.GaussianBlur(radius=severity * 4)))


def add_glare(img_np, severity):
    h, w = img_np.shape
    result = img_np.copy().astype(np.float32)
    for _ in range(random.randint(1, int(2 + severity * 4))):
        cx, cy = random.randint(0, w - 1), random.randint(0, h - 1)
        rx, ry = random.randint(10, int(w * 0.3)), random.randint(5, int(h * 0.2))
        Y, X = np.ogrid[:h, :w]
        dist = ((X - cx) ** 2 / rx ** 2 + (Y - cy) ** 2 / ry ** 2)
        result += np.exp(-dist * 2) * severity * 220
    return np.clip(result, 0, 255).astype(np.uint8)


def add_occlusion(img_np, severity):
    h, w = img_np.shape
    result = img_np.copy()
    for _ in range(random.randint(1, int(1 + severity * 3))):
        bw = random.randint(10, int(w * 0.3))
        bh = random.randint(10, int(h * 0.3))
        bx, by = random.randint(0, max(1, w - bw)), random.randint(0, max(1, h - bh))
        result[by:by + bh, bx:bx + bw] = random.randint(0, 80)
    return result


def add_perspective(img_np, severity):
    h, w = img_np.shape
    img = Image.fromarray(img_np)
    angle = random.uniform(-25, 25) * severity
    img = img.rotate(angle, expand=False, fillcolor=255)
    return np.array(img)


def add_dirty(img_np, severity):
    """Add dirt/stain spots."""
    h, w = img_np.shape
    result = img_np.copy().astype(np.float32)
    for _ in range(random.randint(5, int(10 + severity * 20))):
        cx, cy = random.randint(0, w - 1), random.randint(0, h - 1)
        r = random.randint(3, int(severity * 25))
        Y, X = np.ogrid[:h, :w]
        dist = np.sqrt((X - cx) ** 2 + (Y - cy) ** 2)
        mask = np.clip(1 - dist / r, 0, 1)
        result -= mask * random.uniform(40, 120)
    return np.clip(result, 0, 255).astype(np.uint8)


def add_tear(img_np, severity):
    """Add simulated tear/rip."""
    h, w = img_np.shape
    result = img_np.copy()
    tear_y = random.randint(h // 3, 2 * h // 3)
    tear_w = random.randint(int(w * 0.2 * severity), int(w * 0.6 * severity))
    tear_x = random.randint(0, w - tear_w)
    for x in range(tear_x, tear_x + tear_w):
        offset = random.randint(-3, 3)
        py = tear_y + offset
        if 0 <= py < h:
            result[py, x] = 0
    return result


def generate_scenario(name, count, transform_fn, id_list, zone_list):
    """Generate `count` images for a scenario."""
    out_dir = f"data/synthetic/{name}"
    os.makedirs(f"{out_dir}/images", exist_ok=True)

    rows = []
    for i in range(count):
        pkg_id = random.choice(id_list)
        zone = random.choice(zone_list)
        img_np = make_label_image(pkg_id)
        if transform_fn:
            severity = random.uniform(0.2, 1.0)
            img_np = transform_fn(img_np, severity)
        fname = f"{name}_{i:04d}.png"
        Image.fromarray(img_np).save(f"{out_dir}/images/{fname}")
        rows.append((fname, pkg_id, zone))

    with open(f"{out_dir}/labels.csv", "w") as f:
        f.write("image,package_id,current_zone\n")
        for fname, pkg_id, zone in rows:
            f.write(f"{fname},{pkg_id},{zone}\n")
    print(f"  {name}: {count} images")


def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(os.path.join(base_dir, ".."))

    print("Generating synthetic anomaly label dataset...\n")

    # 1. normal (100)
    generate_scenario("normal", 100, None, QR_PATTERNS, ["A", "A", "A", "B", "C"])

    # 2. wrong_sort (60)
    generate_scenario("wrong_sort", 60, None, QR_PATTERNS, ["B", "C", "A", "A", "A"])

    # 3. qr_occlusion (100)
    generate_scenario("qr_occlusion", 100,
                      lambda img, s: add_occlusion(img, s * 1.5),
                      QR_PATTERNS, ["A"])

    # 4. blur (100)
    generate_scenario("blur", 100, add_blur, QR_PATTERNS, ["A"])

    # 5. glare (80)
    generate_scenario("glare", 80, add_glare, QR_PATTERNS, ["A"])

    # 6. perspective (80)
    generate_scenario("perspective", 80, add_perspective, QR_PATTERNS, ["A"])

    # 7. ocr_confuse (120)
    def confuse_text(img_np, severity):
        # Replace the label text with confusable chars
        confuse_ids = ["PKG00I", "PKGOO1", "PKG0O1", "PKGO01", "PKG0OI"]
        pkg = random.choice(confuse_ids)
        return make_label_image(pkg)
    generate_scenario("ocr_confuse", 120, confuse_text, ["ignored"], ["A"])

    # 8. unknown (80)
    generate_scenario("unknown", 80, None,
                      ["PKG999", "PKG888", "PKG777", "PKG666", "PKG555"], ["A"])

    # 9. unreadable (80)
    def make_unreadable(img_np, severity):
        h, w = img_np.shape
        result = img_np.copy()
        # Heavy blur + noise
        result = add_blur(result, 1.0)
        noise = np.random.normal(0, 60, result.shape).astype(np.int16)
        result = np.clip(result.astype(np.int16) + noise, 0, 255).astype(np.uint8)
        if severity > 0.5:
            # Add black blocks
            for _ in range(random.randint(2, 5)):
                bx = random.randint(0, w - 40)
                by = random.randint(0, h - 30)
                result[by:by + 30, bx:bx + 40] = 0
        return result
    generate_scenario("unreadable", 80, make_unreadable, ["PKG001"], ["A"])

    # 10. dirty_torn (100)
    def dirty_torn(img_np, severity):
        result = add_dirty(img_np, severity)
        if severity > 0.4:
            result = add_tear(result, severity * 0.7)
        return result
    generate_scenario("dirty_torn", 100, dirty_torn, QR_PATTERNS, ["A"])

    total = 100 + 60 + 100 + 100 + 80 + 80 + 120 + 80 + 80 + 100
    print(f"\nTotal: {total} synthetic images")
    print("Done.")


if __name__ == "__main__":
    main()
