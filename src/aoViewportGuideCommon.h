#pragma once
#include <algorithm>

namespace AoViewportGuide
{
    static constexpr const char* kVersion = "v0.3.1";

    static constexpr const char* kOverrideNameInternal = "aoViewportGuideOverride";
    static constexpr const char* kOverrideUiName       = "AO Viewport Guide";

    static constexpr const char* kSettingsNodeTypeName = "aoViewportGuideSettings";
    static constexpr const char* kSettingsNodeName     = "aoViewportGuideSettings1";

    inline float clampf(float v, float lo, float hi)
    {
        return (std::max)(lo, (std::min)(hi, v));
    }
}
