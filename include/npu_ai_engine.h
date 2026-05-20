#pragma once
#include "ai_engine.h"
// Factory: create NPU AI engine (TFLite + Ethos-U Delegate)
// Falls back to CPU if NPU unavailable
IAIEngine* createNPUAIEngine(const std::string& modelPath, const std::string& delegatePath);
