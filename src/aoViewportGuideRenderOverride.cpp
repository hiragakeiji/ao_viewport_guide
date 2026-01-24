#include "aoViewportGuideRenderOverride.h"

using namespace MHWRender;

AOViewportGuideRenderOverride::AOViewportGuideRenderOverride(const MString& name)
: MRenderOverride(name)
, mScene("aoViewportGuideScene")
, mOverlay("aoViewportGuideOverlay")
, mPresent("aoViewportGuidePresent")
{
    // scene operation defaults are fine for v0.2
}

DrawAPI AOViewportGuideRenderOverride::supportedDrawAPIs() const
{
    // ★ ここ超重要：CoreProfile も許可
    return (kOpenGL | kOpenGLCoreProfile | kDirectX11);
}

bool AOViewportGuideRenderOverride::startOperationIterator()
{
    mIndex = 0;
    return true;
}

MRenderOperation* AOViewportGuideRenderOverride::renderOperation()
{
    switch (mIndex)
    {
    case 0: return &mScene;    // render scene
    case 1: return &mOverlay;  // overlay guide
    case 2: return &mPresent;  // present to screen
    default: return nullptr;
    }
}

bool AOViewportGuideRenderOverride::nextRenderOperation()
{
    ++mIndex;
    return (mIndex < 3);
}

MStatus AOViewportGuideRenderOverride::setup(const MString&)
{
    return MS::kSuccess;
}

MStatus AOViewportGuideRenderOverride::cleanup()
{
    return MS::kSuccess;
}
