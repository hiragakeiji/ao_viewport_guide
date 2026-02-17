// aoViewportGuideGate.cpp
// AO Viewport Guide v0.2.3

#include "aoViewportGuideGate.h"
#include "aoViewportGuideSettings.h"

#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>

#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>

#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MColor.h>

#include <algorithm>

static inline float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}
static inline double clampd(double v, double lo, double hi)
{
    return std::max(lo, std::min(hi, v));
}

// correct rect2d call helper (bounds -> rect2d(center, up, w, h, filled))
static inline void rect2d_bounds(MHWRender::MUIDrawManager& dm,
                                double l, double b, double r, double t,
                                bool filled)
{
    const double w = std::max(0.0, r - l);
    const double h = std::max(0.0, t - b);
    if (w <= 0.0 || h <= 0.0) return;

    const double cx = (l + r) * 0.5;
    const double cy = (b + t) * 0.5;
    dm.rect2d(MPoint(cx, cy), MVector(0.0, 1.0, 0.0), w, h, filled);
}

static bool getDefaultResolutionAspect(double& outAspect)
{
    outAspect = 1.0;

    MSelectionList sl;
    if (sl.add("defaultResolution") != MS::kSuccess) return false;

    MObject nodeObj;
    if (sl.getDependNode(0, nodeObj) != MS::kSuccess) return false;

    MFnDependencyNode fn(nodeObj);

    MPlug wPlug = fn.findPlug("width", true);
    MPlug hPlug = fn.findPlug("height", true);
    if (wPlug.isNull() || hPlug.isNull()) return false;

    const int w = wPlug.asInt();
    const int h = hPlug.asInt();
    if (h <= 0) return false;

    outAspect = static_cast<double>(w) / static_cast<double>(h);
    return (outAspect > 0.000001);
}

struct CameraGateParams
{
    bool   ok = false;
    double overscan = 1.0;
    int    filmFit = 3; // 0 Fill, 1 Horizontal, 2 Vertical, 3 Overscan
};

static CameraGateParams getCameraGateParams(const MHWRender::MFrameContext& frameContext)
{
    CameraGateParams p;

    MStatus stat;
    MDagPath camPath = frameContext.getCurrentCameraPath(&stat);
    if (!stat) return p;

    MFnCamera camFn(camPath, &stat);
    if (!stat) return p;

    p.ok = true;
    p.overscan = camFn.overscan();
    p.filmFit = static_cast<int>(camFn.filmFit());
    return p;
}

// IMPORTANT:
// This returns viewport-local coordinates (0..vpW / 0..vpH).
GateRect computeGateRect(const MHWRender::MFrameContext& frameContext,
                         int /*vpX*/, int /*vpY*/, int vpW, int vpH,
                         bool followResolutionGate)
{
    GateRect gate{};

    const double viewW = static_cast<double>(vpW);
    const double viewH = static_cast<double>(vpH);
    if (viewW <= 1.0 || viewH <= 1.0) return gate;

    const double viewAR = viewW / viewH;

    double gateAR = viewAR;
    if (followResolutionGate)
    {
        double ar = 1.0;
        if (getDefaultResolutionAspect(ar))
            gateAR = ar;
    }

    const CameraGateParams camP = getCameraGateParams(frameContext);
    const double overscan = (camP.ok && camP.overscan > 0.0001) ? camP.overscan : 1.0;
    const int filmFit = camP.ok ? camP.filmFit : 3;

    double rectW = viewW;
    double rectH = viewH;

    switch (filmFit)
    {
    case 1: // Horizontal
        rectW = viewW;
        rectH = rectW / gateAR;
        break;
    case 2: // Vertical
        rectH = viewH;
        rectW = rectH * gateAR;
        break;
    case 0: // Fill (cover)
        if (viewAR >= gateAR) { rectW = viewW; rectH = rectW / gateAR; }
        else                 { rectH = viewH; rectW = rectH * gateAR; }
        break;
    case 3: // Overscan (contain)
    default:
        if (viewAR >= gateAR) { rectH = viewH; rectW = rectH * gateAR; }
        else                  { rectW = viewW; rectH = rectW / gateAR; }
        break;
    }

    // overscan shrink
    rectW /= overscan;
    rectH /= overscan;

    // center (LOCAL)
    const double rectX = (viewW - rectW) * 0.5;
    const double rectY = (viewH - rectH) * 0.5;

    gate.left   = rectX;
    gate.bottom = rectY;
    gate.right  = rectX + rectW;
    gate.top    = rectY + rectH;

    return gate;
}

void AoViewportGuideGateOperation::addUIDrawables(MHWRender::MUIDrawManager& dm,
                                                  const MHWRender::MFrameContext& frameContext)
{
    const AoViewportGuideSettingsData s = AoViewportGuideSettings::read();
    if (!s.enable) return;

    int vpX=0, vpY=0, vpW=0, vpH=0;
    frameContext.getViewportDimensions(vpX, vpY, vpW, vpH);
    if (vpW < 10 || vpH < 10) return;

    const GateRect gate = computeGateRect(frameContext, vpX, vpY, vpW, vpH, s.followResolutionGate);

    const double viewL = 0.0;
    const double viewB = 0.0;
    const double viewR = static_cast<double>(vpW);
    const double viewT = static_cast<double>(vpH);

    // draw on top
    dm.beginDrawable();

    // outside matte
    if (s.matteEnable && s.matteOpacity > 0.0001f)
    {
        MColor mc = s.matteColor;
        mc.a = clampf(s.matteOpacity, 0.0f, 1.0f);
        dm.setColor(mc);

        const double left   = clampd(gate.left,   viewL, viewR);
        const double right  = clampd(gate.right,  viewL, viewR);
        const double bottom = clampd(gate.bottom, viewB, viewT);
        const double top    = clampd(gate.top,    viewB, viewT);

        // 4 rectangles around gate
        if (bottom > viewB) rect2d_bounds(dm, viewL, viewB, viewR, bottom, true);
        if (top    < viewT) rect2d_bounds(dm, viewL, top,  viewR, viewT,  true);
        if (left   > viewL) rect2d_bounds(dm, viewL, bottom, left, top, true);
        if (right  < viewR) rect2d_bounds(dm, right, bottom, viewR, top, true);
    }

    // gate border
    if (s.gateBorderEnable && s.gateBorderOpacity > 0.0001f)
    {
        MColor bc = s.gateBorderColor;
        bc.a = clampf(s.gateBorderOpacity, 0.0f, 1.0f);

        dm.setColor(bc);
        dm.setLineWidth(s.gateBorderThickness);

        dm.line2d(MPoint(gate.left,  gate.bottom), MPoint(gate.right, gate.bottom));
        dm.line2d(MPoint(gate.right, gate.bottom), MPoint(gate.right, gate.top));
        dm.line2d(MPoint(gate.right, gate.top),    MPoint(gate.left,  gate.top));
        dm.line2d(MPoint(gate.left,  gate.top),    MPoint(gate.left,  gate.bottom));
    }

    // thirds lines
    {
        MColor lc = s.lineColor;
        lc.a = clampf(s.lineOpacity, 0.0f, 1.0f);

        dm.setColor(lc);
        dm.setLineWidth(s.lineThickness);

        const double w = gate.right - gate.left;
        const double h = gate.top   - gate.bottom;

        const double x1 = gate.left + w / 3.0;
        const double x2 = gate.left + w * 2.0 / 3.0;
        const double y1 = gate.bottom + h / 3.0;
        const double y2 = gate.bottom + h * 2.0 / 3.0;

        dm.line2d(MPoint(x1, gate.bottom), MPoint(x1, gate.top));
        dm.line2d(MPoint(x2, gate.bottom), MPoint(x2, gate.top));
        dm.line2d(MPoint(gate.left, y1),   MPoint(gate.right, y1));
        dm.line2d(MPoint(gate.left, y2),   MPoint(gate.right, y2));
    }

    dm.endDrawable();
}
