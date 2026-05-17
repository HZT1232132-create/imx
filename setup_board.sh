#!/bin/bash
# EdgeGuard-Sort — FRDM-i.MX93 Debian 板端一键部署脚本
# 用法: chmod +x setup_board.sh && ./setup_board.sh

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[SETUP]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC}  $1"; }
err()  { echo -e "${RED}[ERROR]${NC} $1"; }

echo "========================================"
echo " EdgeGuard-Sort 板端部署 (Debian ARM64)"
echo "========================================"
echo ""

# ── 1. System check ──
log "Checking system..."
ARCH=$(uname -m)
if [ "$ARCH" != "aarch64" ]; then
    warn "Architecture is $ARCH, expected aarch64. Continuing anyway..."
fi
echo "  Arch: $ARCH, Cores: $(nproc)"

# ── 2. Install dependencies ──
log "Installing build dependencies..."
sudo apt update
sudo apt install -y cmake build-essential pkg-config \
    libopencv-dev libtesseract-dev libleptonica-dev libssl-dev

log "Verifying key packages..."
for pkg in cmake opencv tesseract; do
    if dpkg -l | grep -q "$pkg"; then
        echo "  $pkg: OK"
    else
        warn "$pkg may not be installed"
    fi
done

# ── 3. ONNX Runtime (ARM64) ──
ONNX_VER="1.20.1"
ONNX_TGZ="onnxruntime-linux-aarch64-${ONNX_VER}.tgz"
ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VER}/${ONNX_TGZ}"

if [ -f "lib/libonnxruntime.so" ] && file lib/libonnxruntime.so | grep -q "ARM aarch64"; then
    log "ARM64 ONNX Runtime already present."
else
    log "Downloading ONNX Runtime ${ONNX_VER} for ARM64..."
    if [ ! -f "$ONNX_TGZ" ]; then
        wget -q --show-progress "$ONNX_URL" || {
            warn "Download failed. ONNX Runtime (TinyOCR mode) will be unavailable."
            warn "You can still run with mode=full (Tesseract OCR)."
            ONNX_MISSING=1
        }
    fi

    if [ -z "$ONNX_MISSING" ] && [ -f "$ONNX_TGZ" ]; then
        tar xzf "$ONNX_TGZ"
        cp "onnxruntime-linux-aarch64-${ONNX_VER}/lib/"* lib/
        rm -rf "onnxruntime-linux-aarch64-${ONNX_VER}"
        log "ONNX Runtime ARM64 installed to lib/"
    fi
fi

# ── 4. Build ──
log "Building project..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

if [ -f "build/sorting_sim" ]; then
    log "Build successful! Binary: build/sorting_sim"
    file build/sorting_sim
else
    err "Build failed — check errors above."
    exit 1
fi

# ── 5. Quick verify ──
log "Creating output/log directories..."
mkdir -p output logs

echo ""
echo "========================================"
echo -e " ${GREEN}Deployment complete!${NC}"
echo "========================================"
echo ""
echo "Run modes:"
echo "  ./build/sorting_sim                              # full mode (Tesseract OCR)"
echo "  ./build/sorting_sim '' '' '' 0 full               # same as above"
echo "  ./build/sorting_sim '' '' '' 0 tinyocr            # Tiny-ID-OCR (ONNX)"
echo "  ./build/sorting_sim '' '' '' 0 baseline           # QR only"
echo "  ./build/sorting_sim '' '' '' 0 ocr                # QR + OCR"
echo ""
echo "Validate results:"
echo "  python3 validate.py logs/events.csv data/expected.csv"
echo ""
