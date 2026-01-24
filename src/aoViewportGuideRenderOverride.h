#pragma once
#include <maya/MRenderOverride.h>
#include <maya/MSceneRender.h>
#include <maya/MPresentTarget.h>

#include "aoViewportGuideOverlayOp.h"

class AOViewportGuideRenderOverride : public MHWRender::MRenderOverride
{
public:
    explicit AOViewportGuideRenderOverride(const MString& name);
    ~AOViewportGuideRenderOverride() override = default;

    MHWRender::DrawAPI supportedDrawAPIs() const override;

    bool startOperationIterator() override;
    MHWRender::MRenderOperation* renderOperation() override;
    bool nextRenderOperation() override;

    MStatus setup(const MString& destination) override;
    MStatus cleanup() override;

private:
    int mIndex = 0;

    // ops
    MHWRender::MSceneRender   mScene;
    AOViewportGuideOverlayOp  mOverlay;
    MHWRender::MPresentTarget mPresent;
};
