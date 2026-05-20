"""
Generate 6 test images for EdgeGuard-Sort.
Each image covers a different recognition/sort scenario.
Requires: pip install qrcode[pil]
"""
import qrcode
import random
import os
from PIL import Image, ImageDraw, ImageFont

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(BASE_DIR, "data", "images")
os.makedirs(OUT_DIR, exist_ok=True)

def _get_fonts():
    try:
        ft = ImageFont.truetype("arial.ttf", 24)
        ftext = ImageFont.truetype("arial.ttf", 20)
    except:
        ft = ImageFont.load_default()
        ftext = ImageFont.load_default()
    return ft, ftext

def _damage_qr(qr_img):
    draw = ImageDraw.Draw(qr_img)
    draw.rectangle([30, 30, 80, 100], fill=(255, 255, 255))
    draw.rectangle([80, 90, 150, 130], fill=(255, 255, 255))
    for _ in range(600):
        x, y = random.randint(100, 170), random.randint(50, 150)
        c = random.randint(0, 255)
        draw.point((x, y), fill=(c, c, c))
    return qr_img

def create_label_image(qr_data, text_label, filename, qr_broken=False):
    img = Image.new("RGB", (640, 400), (255, 255, 255))
    draw = ImageDraw.Draw(img)
    font_title, font_text = _get_fonts()

    draw.rectangle([5, 5, 635, 395], outline=(0, 0, 0), width=2)
    draw.text((20, 15), "WAREHOUSE PACKAGE LABEL", fill=(0, 0, 0), font=font_title)
    draw.text((20, 50), f"ID: {text_label}", fill=(0, 0, 0), font=font_text)
    draw.text((20, 80), "Type: Standard Package", fill=(100, 100, 100), font=font_text)
    draw.rectangle([15, 115, 625, 380], outline=(200, 200, 200), width=1)

    if qr_data:
        qr = qrcode.QRCode(version=2, box_size=6, border=4)
        qr.add_data(qr_data)
        qr.make(fit=True)
        qr_img = qr.make_image(fill_color="black", back_color="white").convert("RGB")
        if qr_broken:
            qr_img = _damage_qr(qr_img)
        qr_img = qr_img.resize((180, 180))
        img.paste(qr_img, (400, 150))
    else:
        draw.rectangle([400, 150, 580, 330], fill=(240, 240, 240), outline=(200, 200, 200))
        draw.text((420, 220), "QR CODE", fill=(200, 200, 200), font=font_text)
        draw.text((420, 245), "DAMAGED", fill=(200, 200, 200), font=font_text)

    draw.text((400, 340), text_label, fill=(80, 80, 80), font=font_text)

    path = os.path.join(OUT_DIR, filename)
    img.save(path)
    print(f"Created: {filename}")
    return path

# ============================================================
# Frame 1: 正常 PKG001 → QR_SUCCESS → A区
create_label_image("PKG001", "PKG001", "pkg001_normal.png")
print("  → QR_SUCCESS | A区 | PASS\n")

# Frame 2: PKG002 不在规则库 → UNKNOWN → 复核区
create_label_image("PKG002", "PKG002", "pkg002_wrong.png")
print("  → UNKNOWN_PACKAGE | 复核区 | REVIEW\n")

# Frame 3: PKG003 QR损坏 → OCR救回 → B区
create_label_image("PKG003", "PKG003", "pkg003_qr_damaged.png", qr_broken=True)
print("  → OCR_RECOVERED | B区 | PASS_WITH_LOG\n")

# Frame 4: 无QR, 文字PKG004带混淆(O→0) → OCR读出 → 规则纠错 → C区
create_label_image(None, "PKGOO4", "pkg004_ocr_error.png")
print("  → OCR_CORRECTED | C区 | PASS_WITH_LOG\n")

# Frame 5: PKG999 不在规则库 → UNKNOWN → 复核区
create_label_image("PKG999", "PKG999", "pkg005_unknown.png")
print("  → UNKNOWN_PACKAGE | 复核区 | REVIEW\n")

# Frame 6: 无QR, 乱码文字 → 完全不可读 → 复核区
create_label_image(None, "X?#*!%@$", "pkg006_unreadable.png")
print("  → LABEL_ERROR | 复核区 | BLOCK\n")

print(f"Done! Images in: {OUT_DIR}")
