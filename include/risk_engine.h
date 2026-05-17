#pragma once

#include <string>

enum class IdStatus {
    QR_SUCCESS,       // 二维码/条形码识别成功
    OCR_RECOVERED,    // 主识别失败，OCR直接识别成功且规则匹配
    OCR_CORRECTED,    // OCR结果经编辑距离纠正后匹配
    UNKNOWN_PACKAGE,  // 识别到文本但无法匹配任何已知货物
    LABEL_ERROR       // 主识别和OCR均失败
};

enum class SortStatus {
    NORMAL_SORT,   // 目标分拣区 == 当前分拣区
    WRONG_SORT,    // 目标分拣区 != 当前分拣区
    CANNOT_JUDGE   // 标签异常/未知货物，无法判断目标区
};

enum class RiskLevel {
    LEVEL_0_NORMAL,    // QR成功 + 正确分拣
    LEVEL_1_LOW,       // OCR补救成功，低风险提示
    LEVEL_2_MEDIUM,     // OCR纠正成功，需复核
    LEVEL_3_HIGH,      // 未知货物/标签异常
    LEVEL_4_CRITICAL   // 错误异常，需立即告警
};

class RiskEngine {
public:
    RiskLevel map(IdStatus idStatus, SortStatus sortStatus) const;
    const char* levelName(RiskLevel level) const;
    const char* idStatusName(IdStatus status) const;
    const char* sortStatusName(SortStatus status) const;
};
