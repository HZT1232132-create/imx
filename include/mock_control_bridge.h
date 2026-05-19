#pragma once
#include "control_bridge.h"
// Factory: create Mock M33 control bridge (virtual IO, no real M33)
IControlBridge* createMockControlBridge();
