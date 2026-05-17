"""
Generate 6 test images for the iMX93 sorting simulator.
Each image contains a QR code (or damaged QR) and text representing different package scenarios.

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
    """Get title and text fonts."""
    try:
        ft = ImageFont.truetype("arial.ttf", 24)
        ftext = ImageFont.truetype("arial.ttf", 20)
    except:
        ft = ImageFont.load_default()
        ftext = ImageFont.load_default()
    return ft, ftext


def _damage_qr(qr_img):
    """Use PIL to draw white blocks and noise over QR code to simulate damage."""
    draw = ImageDraw.Draw(qr_img)
    # White blocks simulating physical damage
    draw.rectangle([30, 30, 80, 100], fill=(255, 255, 255))
    draw.rectangle([80, 90, 150, 130], fill=(255, 255, 255))
    # Random noise patches
    for x in range(100, 120):
        for y in range(50, 70):
            c = random.randint(0, 255)
            draw.point((x, y), fill=(c, c, c))
    return qr_img


def create_label_image(qr_data, text_label, filename, qr_broken=False):
    """Create a 640x400 package label image with QR code and text."""
    img = Image.new("RGB", (640, 400), (255, 255, 255))
    draw = ImageDraw.Draw(img)
    font_title, font_text = _get_fonts()

    # Draw border
    draw.rectangle([5, 5, 635, 395], outline=(0, 0, 0), width=2)

    # Header
    draw.text((20, 15), "WAREHOUSE PACKAGE LABEL", fill=(0, 0, 0), font=font_title)
    draw.text((20, 50), f"ID: {text_label}", fill=(0, 0, 0), font=font_text)
    draw.text((20, 80), "Type: Standard Package", fill=(100, 100, 100), font=font_text)
    draw.rectangle([15, 115, 625, 380], outline=(200, 200, 200), width=1)

    # Generate QR code
    if qr_data:
        qr = qrcode.QRCode(version=2, box_size=6, border=4)
        qr.add_data(qr_data)
        qr.make(fit=True)
        qr_img = qr.make_image(fill_color="black", back_color="white").convert("RGB")

        if qr_broken:
            qr_img = _damage_qr(qr_img)

        # Paste QR code onto label (right side)
        qr_img = qr_img.resize((180, 180))
        img.paste(qr_img, (400, 150))
    else:
        # No QR — damaged/blank area
        draw.rectangle([400, 150, 580, 330], fill=(240, 240, 240), outline=(200, 200, 200))
        draw.text((420, 220), "QR CODE", fill=(200, 200, 200), font=font_text)
        draw.text((420, 245), "DAMAGED", fill=(200, 200, 200), font=font_text)

    # Text label below QR
    draw.text((400, 340), text_label, fill=(80, 80, 80), font=font_text)

    # Save
    path = os.path.join(OUT_DIR, filename)
    img.save(path)
    print(f"Created: {path}")
    return path


# ============================================================
# Generate 6 test images
# ============================================================

# 1. Normal PKG001 QR code, correct zone
create_label_image("PKG001", "PKG001", "pkg001_normal.png")
print("  -> Expected: QR_SUCCESS + NORMAL_SORT + Level 0\n")

# 2. PKG002 QR code, but current zone is A (target zone B)
create_label_image("PKG002", "PKG002", "pkg002_wrong.png")
print("  -> Expected: QR_SUCCESS + WRONG_SORT + Level 4\n")

# 3. Damaged QR code, OCR-friendly text "PKG003" visible
create_label_image("PKG003", "PKG003", "pkg003_qr_damaged.png", qr_broken=True)
print("  -> Expected: OCR_RECOVERED + Level 1\n")

# 4. NO QR code at all — only printed text PKG00I (letter I instead of 1)
#    QR completely fails → OCR reads PKG00I → corrector fixes to PKG001
create_label_image(None, "PKG00I", "pkg004_ocr_error.png")
print("  -> Expected: QR fails → OCR reads PKG00I → OCR_CORRECTED → PKG001 + Level 2\n")

# 5. PKG999 QR code, not present in rules.csv
create_label_image("PKG999", "PKG999", "pkg005_unknown.png")
print("  -> Expected: UNKNOWN_PACKAGE + Level 3\n")

# 6. No QR code at all, garbled text unreadable
create_label_image(None, "X?Z#*!", "pkg006_unreadable.png")
print("  -> Expected: LABEL_ERROR + Level 3\n")

print("Done! All 6 test images generated in:", OUT_DIR)
