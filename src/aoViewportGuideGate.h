#pragma once
#include <maya/MFrameContext.h>

namespace AoViewportGuide
{
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
}
