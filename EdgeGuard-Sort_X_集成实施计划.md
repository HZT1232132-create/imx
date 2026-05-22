# EdgeGuard-Sort X 集成实施计划

## 方案融合来源

| 文档 | 核心贡献 |
|------|---------|
| EdgeGuard-Sort_X V3 | 双源校验 + WMS+ TrustFusion 2.0 + R0-R5 + 多帧一致性 + 220张数据集管线 |
| EdgeGuard-Sort_可信标签 V2 | 标签健康度 + 置信度融合 + R0-R4 + Web分拣仪表板 |
| 当前实现 | ZBar/OCR/纠错/质量/决策/Web/M33/NPU 已全栈跑通 |
| 420_2 数据集 | 220张真实快递面单, VN=82, PH=12, CN=11, 100%条码可扫 |

---

## 一、核心定位

**不只是识别标签 → 是判断标签是否可信、来源是否一致、订单是否匹配，再给出可解释的分拣决策。**

| 旧定位 | 新定位 |
|--------|--------|
| QR/OCR 识别 + 简单规则 | 双源校验 + WMS校核 + TrustFusion + 异常追溯 |
| 6张合成图演示 | 220张真实数据 + 自动验收 |
| 单帧判断 | 多帧一致性投票 |
| 只看标签 | 标签 + 外箱文字交叉验证 |

---

## 二、升级后的系统架构

```
摄像头/图片帧 (1-5帧)
        ↓
┌─ Stage 0: YOLO 检测 (NPU) ──────────────────────────┐
│  检测左上角 small_order_label → bbox + confidence    │
│  Ethos-U65 NPU 推理 (Vela编译 TFLite INT8)           │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─ Stage 1: ROI 裁剪 + 标签健康度 ────────────────────┐
│  裁剪 label ROI, 评估: normal/blur/glare/occlusion/  │
│  barcode_damage/unreadable → health_score            │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─ Stage 2: 双通道识别 ───────────────────────────────┐
│  通道A: ZBar 条码 → origin_code (VN/CN/PH/JP/TH/XX) │
│  通道B: OCR 文字 → origin_text (MADE IN VIETNAM...)  │
│  任一失败 → 另一通道兜底                             │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─ Stage 3: 纠错 + 产地确认 ──────────────────────────┐
│  OCR混淆归一化 (O→0, I→1, V1ETNAM→VIETNAM)          │
│  产地代码映射: VN/CN/PH → 目标区 | JP/TH/XX → 复核  │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─ Stage 4: 外箱文字OCR (双源校验) ───────────────────┐
│  全图OCR识别 MADE IN VIETNAM/CHINA/PHILIPPINES       │
│  与 Stage 3 产地代码交叉验证                          │
│  一致 → 高可信 / 冲突 → REVIEW                       │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─ Stage 5: WMS 订单校核 ─────────────────────────────┐
│  order_id → orders.csv → expected_origin, status     │
│  REJECTED → 硬阻止 / 不匹配 → REVIEW / 缺订单 → 人工  │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─ Stage 6: TrustFusion 2.0 ──────────────────────────┐
│  7因子加权融合 → R0-R5 风险等级                       │
│  0.20 YOLO detection confidence                      │
│  0.15 Label health score                             │
│  0.20 Barcode decode confidence                      │
│  0.10 OCR confidence                                 │
│  0.15 Carton origin consistency (双源校验)            │
│  0.15 WMS rule match                                 │
│  0.05 Temporal consistency (多帧投票)                 │
│                                                      │
│  R0 → PASS (绿灯, 高可信, 直达目标区)                 │
│  R1 → PASS_WITH_LOG (绿灯+日志, 低风险)              │
│  R2 → PASS_WITH_LOG (黄灯, 需减速监控)               │
│  R3 → REVIEW (黄灯, 双源冲突/标签异常)               │
│  R4 → BLOCK_REVIEW (红灯, WMS拒绝/严重异常)          │
│  R5 → BLOCK_ALARM (红灯+蜂鸣, 完全不可读/紧急停机)   │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─ Stage 7: 多帧一致性投票 ───────────────────────────┐
│  同纸箱3-5帧 → 投票确认最终决策                       │
│  4/5一致 → 通过 / 不稳定 → REVIEW                    │
└────────────────────┬────────────────────────────────┘
                     ↓
┌─ 输出层 ────────────────────────────────────────────┐
│  ├─ GPIO告警 (M33/libgpiod)                          │
│  ├─ HDMI HUD                                         │
│  ├─ Web 运营控制台 (7面板)                            │
│  ├─ events.csv / latest_event.json                   │
│  ├─ hard_cases/ 异常自动回收                          │
│  └─ expected_results.csv 自动验收                     │
└─────────────────────────────────────────────────────┘
```

---

## 三、路由规则

| 产地代码 | 目标区 | 条件 |
|---------|--------|------|
| VN (越南) | VN区 | 双源一致 + WMS匹配 + R0 |
| CN (中国) | CN区 | 双源一致 + WMS匹配 + R0 |
| PH (菲律宾) | PH区 | 双源一致 + WMS匹配 + R0 |
| JP/TH/XX/MY | 复核区 | 非标准来源 |
| UNKNOWN | 复核区 | 条码/OCR均失败 |
| 双源冲突 | 复核区 | 标签 ≠ 外箱文字 |

---

## 四、TrustFusion 2.0 硬阻断规则

| 条件 | 阻断 | 理由 |
|------|------|------|
| WMS status = REJECTED | 禁止PASS | 订单已拒绝 |
| 双源校验冲突 (VN≠CN) | 禁止PASS | 可能贴错标签 |
| YOLO未检测到标签 | 禁止PASS | 标签缺失 |
| OCR成功但代码为JP/TH/XX | 禁止PASS | 非标准来源 |
| QR+OCR+外箱OCR全失败 | 禁止PASS | 完全不可读 |

---

## 五、数据集管线 (已生成)

```
EdgeGuard_Dataset/ (1.6GB, 5114文件)
├── detect_aug_yolo/    1694+33+33 (train/val/test, 11倍增强)
├── label_health_cls/   1386张 (9类×154)
├── ocr_corrector_test/  50张 + 14混淆用例
├── decision_scenes/     150张 (6场景)
├── hard_cases_template/  5目录 + README
└── scripts/              生成脚本
```

---

## 六、版本路线 (V0-V8)

| 版本 | 目标 | 状态 |
|------|------|------|
| V0 | 固定ROI+条码识别+rules.csv路由 | ✅ 已完成 |
| V1 | YOLO small_order_label检测 | ⬅ 当前 |
| V2 | 条码+OCR+标签健康度 | 数据集已就绪 |
| V3 | 外箱文字OCR双源校验 | 待实施 |
| V4 | WMS订单校核 (orders.csv) | 待实施 |
| V5 | TrustFusion 2.0 + R0-R5 | 待实施 |
| V6 | Web运营控制台 (7面板) | 部分完成 |
| V7 | i.MX93 NPU部署 | 库已装, 待YOLO模型 |
| V8 | 答辩演示+自动验收 | 待实施 |

---

## 七、演示场景 (8帧)

| # | 场景 | 标签 | 外箱OCR | WMS | 结果 |
|---|------|------|---------|-----|------|
| 1 | 正常VN | VN | VIETNAM | VN | R0, 绿灯→VN区 |
| 2 | 条码损坏OCR恢复 | VN(OCR) | VIETNAM | VN | R1, 黄灯→VN区 |
| 3 | 双源冲突 | VN | CHINA | CN | R3, 黄灯→复核 |
| 4 | WMS拒绝 | CN | CHINA | REJECTED | R4, 红灯→拒绝 |
| 5 | 标签不可读 | — | — | VN | R5, 红灯蜂鸣→复核 |
| 6 | 未知来源JP | JP | JAPAN | — | R3, 黄灯→复核 |
| 7 | OCR混淆纠错 | PH(PHL) | PHILIPPINES | PH | R2, 黄灯→PH区 |
| 8 | 多帧投票稳定 | VN(5/5) | VIETNAM | VN | R0, 绿灯→VN区 |
