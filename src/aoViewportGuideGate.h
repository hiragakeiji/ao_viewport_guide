#pragma once
// AO Viewport Guide v0.2.3
// Gate + thirds + matte drawing (HUD 2D)

#include <maya/MViewport2Renderer.h>
#include <maya/MFrameContext.h>
#include <maya/MUIDrawManager.h>

struct GateRect
{
    double left   = 0.0;
    double bottom = 0.0;
    double right  = 0.0;
    double top    = 0.0;
};

GateRect computeGateRect(const MHWRender::MFrameContext& frameContext,
                         int vpX, int vpY, int vpW, int vpH,
                         bool followResolutionGate);

// NOTE:
// Your environment may NOT have MHUDRenderOperation.
// This version uses MHWRender::MHUDRender directly (as your working style).
class AoViewportGuideGateOperation : public MHWRender::MHUDRender
{
public:
    bool hasUIDrawables() const override { return true; }

    void addUIDrawables(MHWRender::MUIDrawManager& dm,
                        const MHWRender::MFrameContext& frameContext) override;
};
