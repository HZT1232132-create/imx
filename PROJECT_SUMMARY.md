# EdgeGuard-Sort 项目实现总结

## 项目定位

基于 **NXP FRDM-i.MX93** 开发板的**仓储标签异常鲁棒识别与智能分拣决策终端**。

## 系统架构

```
图片输入 → 主识别(ZBar QR) → 兜底识别(Tesseract OCR) 
→ 规则纠错(编辑距离) → 质量评估(5维) → 异常检测(3类) 
→ 风险定级(0~4) → 决策融合(5因子) → GPIO告警 + HUD显示 + Hash溯源日志
```

## 13 个核心模块

| 模块 | 功能 | 技术实现 |
|------|------|---------|
| InputSource | 图片序列输入 | CSV 驱动，支持扩展摄像头 |
| PrimaryRecognizer | QR 主识别 | ZBar 二维码检测 |
| OCRRecognizer | 文字兜底识别 | Tesseract + 多 ROI 策略 |
| TinyOCREngine | 轻量 OCR | ONNX Runtime, CRNN+CTC, 77ms 推理 |
| OCRCorrector | 规则纠错 | 混淆归一化(O→0,I→1) + Levenshtein 编辑距离 |
| RuleEngine | 货物规则匹配 | CSV 规则库(Package→Zone) |
| QualityEngine | 5 维质量评估 | 模糊度/眩光/倾斜/遮挡/破损 → GOOD/WARNING/BAD |
| AnomalyDetector | 3 类异常检测 | 撕裂(边缘碎片)/污损(自适应残差)/叠贴(直方差异常) |
| RiskEngine | 5 级风险映射 | LEVEL_0~4, 分拣错误直挂 LEVEL_4 |
| DecisionEngine | 5 因子置信度融合 | Quality+Recognition+Rule+Correction+Anomaly → PASS/REVIEW/BLOCK |
| AlarmSimulator | GPIO 真实告警 | libgpiod 控制 gpiochip2 (绿/黄/红 LED + 蜂鸣器) |
| HashLogger | 防篡改溯源 | SHA-256 哈希链, prev_hash→current_hash, 支持全量校验 |
| StatsManager | 实时统计 | 识别率/误放行率/高风险数/决策分布 |

## 版本递进 (V0-V5 消融实验)

| 版本 | 识别率 | 平均延迟 | 高风险事件 |
|------|--------|---------|-----------|
| V0 (QR only) | 33.3% | 236ms | 5/6 |
| V2 (QR+OCR+Corrector+Quality+Decision) | 66.7% | 547ms | 3/6 |
| V4 (TinyOCR CPU) | 66.7% | 558ms | 4/6 |

V2 完整版比 V0 baseline 识别率翻倍、高风险事件降低 40%。

## 部署状态

| 能力 | 状态 |
|------|------|
| FRDM-i.MX93 Debian ARM64 编译运行 | ✅ |
| ZBar QR + Tesseract OCR 识别 | ✅ |
| HDMI HUD 显示 (Weston/Wayland) | ✅ |
| GPIO 真实 LED/蜂鸣器控制 | ✅ (代码就绪，待接线) |
| Hash 防篡改日志链 | ✅ |
| WiFi 一键连接 + HTTP 代码同步 | ✅ |
| GitHub 版本管理 | ✅ |
| TinyOCR ONNX 模型推理 | ✅ (模型质量待改进) |
| USB/MIPI 摄像头 | 待集成 |

## 关键技术点

- **ARM64 交叉兼容**: CMake + pkg-config 适配, ONNX Runtime ARM64 替换 x86
- **ZBar 替代 QUIRC**: NXP Debian 镜像 OpenCV 缺少 QUIRC, 用 ZBar 补位
- **Headless 自动检测**: 无显示器时自动跳过 GUI, 保存标注帧到 output/
- **自适应阈值**: QualityEngine 使用统计分布(3-sigma/变异系数)替代固定阈值
- **Wayland socket 发现**: 自动检测 `/run/user/0/wayland-1` 实现 HDMI 显示

## 板端操作速查

```bash
# 连接 WiFi
./connect_wifi.sh

# 拉代码编译
git pull origin main
cd build && rm -rf * && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

# 运行 (headless)
QT_QPA_PLATFORM=offscreen ./sorting_sim ../data/demo_sequence.csv ../data/rules.csv ../data/images 1

# 运行 (HDMI GUI)
export WAYLAND_DISPLAY=wayland-1
QT_QPA_PLATFORM=wayland-egl ./sorting_sim ../data/demo_sequence.csv ../data/rules.csv ../data/images 0

# 验证日志
python3 validate.py ../logs/events.csv ../data/expected.csv
```

## 仓库地址

https://github.com/HZT1232132-create/imx
