#pragma once
// AO Viewport Guide v0.2.3
// RenderOverride wrapper

#include <maya/MViewport2Renderer.h>

namespace AoViewportGuide
{
    inline constexpr const char* kOverrideInternalName = "aoViewportGuide";
    inline constexpr const char* kOverrideUiName       = "AO Viewport Guide";

    MHWRender::MRenderOverride* createOverride();
    void destroyOverride(MHWRender::MRenderOverride*& p);
}
