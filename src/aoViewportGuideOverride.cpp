// aoViewportGuideOverride.cpp
// AO Viewport Guide v0.2.3
//
// IMPORTANT:
// - Do NOT include <maya/MFnPlugin.h> here

#include "aoViewportGuideOverride.h"
#include "aoViewportGuideGate.h"
#include "aoViewportGuideSettings.h"

#include <maya/MString.h>
#include <maya/MStatus.h>

namespace
{
class AoViewportGuideOverrideImpl : public MHWRender::MRenderOverride
{
public:
    explicit AoViewportGuideOverrideImpl(const MString& name)
        : MHWRender::MRenderOverride(name)
    {
        mScene   = new MHWRender::MSceneRender("aoViewportGuide_scene");
        mGateHUD = new AoViewportGuideGateOperation();
        mPresent = new MHWRender::MPresentTarget("aoViewportGuide_present");

        mOps[0] = mScene;
        mOps[1] = mGateHUD;
        mOps[2] = mPresent;
    }

    ~AoViewportGuideOverrideImpl() override
    {
        delete mScene;   mScene = nullptr;
        delete mGateHUD; mGateHUD = nullptr;
        delete mPresent; mPresent = nullptr;
    }

    MString uiName() const override
    {
        return AoViewportGuide::kOverrideUiName;
    }

    MHWRender::DrawAPI supportedDrawAPIs() const override
    {
        return MHWRender::kAllDevices; // DX11/OpenGL
    }

    MStatus setup(const MString& /*destination*/) override
    {
        // ensure settings node exists
        (void)AoViewportGuideSettings::ensure();
        return MS::kSuccess;
    }

    MStatus cleanup() override
    {
        return MS::kSuccess;
    }

    bool startOperationIterator() override
    {
        mIndex = 0;
        return true;
    }

    MHWRender::MRenderOperation* renderOperation() override
    {
        if (mIndex < 3) return mOps[mIndex];
        return nullptr;
    }

    bool nextRenderOperation() override
    {
        ++mIndex;
        return (mIndex < 3);
    }

private:
    unsigned int mIndex = 0;

    MHWRender::MSceneRender*        mScene   = nullptr;
    AoViewportGuideGateOperation*   mGateHUD = nullptr;
    MHWRender::MPresentTarget*      mPresent = nullptr;

    MHWRender::MRenderOperation* mOps[3] = { nullptr, nullptr, nullptr };
};
} // anon

namespace AoViewportGuide
{
MHWRender::MRenderOverride* createOverride()
{
    return new AoViewportGuideOverrideImpl(kOverrideInternalName);
}

void destroyOverride(MHWRender::MRenderOverride*& p)
{
    delete p;
    p = nullptr;
}
}
