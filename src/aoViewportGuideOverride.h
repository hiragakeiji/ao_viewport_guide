#pragma once
#include <maya/MViewport2Renderer.h>

namespace AoViewportGuide
{
    MHWRender::MRenderOverride* createOverride();
    void destroyOverride(MHWRender::MRenderOverride*& ptr);
}
