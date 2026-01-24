// aoViewportGuideDrawOverride.cpp
// v0.2: Resolution Gate (render aspect) aligned thirds guide
// Maya 2025 / VP2 (MUIDrawManager)

#include "aoViewportGuideDrawOverride.h"
#include "aoViewportGuideLocator.h"

#include <maya/MFnCamera.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MSelectionList.h>
#include <maya/MPlug.h>
#include <maya/MColor.h>
#include <maya/MPoint.h>
#include <maya/MStatus.h>
#include <maya/MObject.h>

using namespace MHWRender;

AOViewportGuideDrawOverride::AOViewportGuideDrawOverride(const MObject& obj)
    : MPxDrawOverride(obj, nullptr, false)
{
}

MPxDrawOverride* AOViewportGuideDrawOverride::creator(const MObject& obj)
{
    return new AOViewportGuideDrawOverride(obj);
}

DrawAPI AOViewportGuideDrawOverride::supportedDrawAPIs() const
{
    return (kOpenGL | kOpenGLCoreProfile | kDirectX11);
}

bool AOViewportGuideDrawOverride::isBounded(const MDagPath&, const MDagPath&) const
{
    return false;
}

MBoundingBox AOViewportGuideDrawOverride::boundingBox(const MDagPath&, const MDagPath&) const
{
    return MBoundingBox();
}

// Required in Maya 2025: prepareForDraw is pure virtual.
// v0.2 doesn't need custom user data, so just pass-through oldData.
MUserData* AOViewportGuideDrawOverride::prepareForDraw(
    const MDagPath&,
    const MDagPath&,
    const MHWRender::MFrameContext&,
    MUserData* oldData)
{
    return oldData;
}
bool AOViewportGuideDrawOverride::hasUIDrawables() const
{
    return true;
}

bool AOViewportGuideDrawOverride::getViewportSize(const MFrameContext& ctx, int& outW, int& outH)
{
    int x = 0, y = 0, w = 0, h = 0;
    const MStatus st = ctx.getViewportDimensions(x, y, w, h);
    if (!st) return false;

    outW = w;
    outH = h;
    return (outW > 0 && outH > 0);
}

bool AOViewportGuideDrawOverride::getCameraInfo(const MFrameContext& ctx, MDagPath& outCamPath)
{
    MStatus st;
    outCamPath = ctx.getCurrentCameraPath(&st);
    if (!st) return false;
    return outCamPath.isValid();
}

bool AOViewportGuideDrawOverride::getRenderAspect(double& outAspect)
{
    // defaultResolution.width / defaultResolution.height
    MSelectionList sl;
    if (sl.add("defaultResolution") != MS::kSuccess) return false;

    MObject node;
    sl.getDependNode(0, node);
    if (node.isNull()) return false;

    MFnDependencyNode fn(node);

    MPlug pW = fn.findPlug("width", true);
    MPlug pH = fn.findPlug("height", true);

    double w = 0.0, h = 0.0;
    pW.getValue(w);
    pH.getValue(h);

    if (h <= 0.0) return false;

    outAspect = w / h;
    return true;
}

static void rectFromCenter(
    int vpW, int vpH,
    double rectW, double rectH,
    double& l, double& b, double& r, double& t)
{
    const double cx = vpW * 0.5;
    const double cy = vpH * 0.5;
    l = cx - rectW * 0.5;
    r = cx + rectW * 0.5;
    b = cy - rectH * 0.5;
    t = cy + rectH * 0.5;
}

void AOViewportGuideDrawOverride::computeResolutionGateRect(
    int vpW, int vpH,
    double gateAspect,
    short filmFit,
    double overscan,
    double& outLeft, double& outBottom, double& outRight, double& outTop)
{
    const double vpAspect = (vpH > 0) ? (double)vpW / (double)vpH : 1.0;

    auto computeContain = [&](double& w, double& h)
    {
        // fit INSIDE viewport (no crop)
        if (vpAspect >= gateAspect) {
            h = (double)vpH;
            w = h * gateAspect;
        } else {
            w = (double)vpW;
            h = w / gateAspect;
        }
    };

    auto computeCover = [&](double& w, double& h)
    {
        // cover viewport (may crop)
        if (vpAspect >= gateAspect) {
            w = (double)vpW;
            h = w / gateAspect;
        } else {
            h = (double)vpH;
            w = h * gateAspect;
        }
    };

    double rectW = 0.0, rectH = 0.0;

    // Maya filmFit:
    // kFillFilmFit(0), kHorizontalFilmFit(1), kVerticalFilmFit(2), kOverscanFilmFit(3)
    // v0.2: "looks right" mapping (exact match can be refined later)
    if (filmFit == MFnCamera::kHorizontalFilmFit) {
        rectW = (double)vpW;
        rectH = rectW / gateAspect;
    }
    else if (filmFit == MFnCamera::kVerticalFilmFit) {
        rectH = (double)vpH;
        rectW = rectH * gateAspect;
    }
    else if (filmFit == MFnCamera::kFillFilmFit) {
        computeCover(rectW, rectH);
    }
    else { // kOverscanFilmFit or unknown
        computeContain(rectW, rectH);
    }

    if (overscan <= 0.0) overscan = 1.0;
    rectW *= overscan;
    rectH *= overscan;

    rectFromCenter(vpW, vpH, rectW, rectH, outLeft, outBottom, outRight, outTop);
}

void AOViewportGuideDrawOverride::addUIDrawables(
    const MDagPath& objPath,
    MHWRender::MUIDrawManager& drawManager,
    const MHWRender::MFrameContext& frameContext,
    const MUserData* /*data*/)
{
    // enable attribute
    bool enabled = true;
    {
        const MObject node = objPath.node();
        if (!node.isNull()) {
            MFnDependencyNode fn(node);
            MPlug p = fn.findPlug(AOViewportGuideLocator::aEnable, true);
            if (!p.isNull()) {
                p.getValue(enabled);
            }
        }
    }
    if (!enabled) return;

    int vpW = 0, vpH = 0;
    if (!getViewportSize(frameContext, vpW, vpH)) return;

    // Gate aspect: use render resolution aspect (defaultResolution)
    double gateAspect = 1.7777777778; // fallback 16:9
    (void)getRenderAspect(gateAspect);

    // Camera info from the current viewport
    MDagPath camPath;
    if (!getCameraInfo(frameContext, camPath)) return;

    MFnCamera cam(camPath);
    const short filmFit = cam.filmFit();
    const double overscan = cam.overscan();

    // Compute resolution gate rect in screen (viewport pixel) space
    double l = 0, b = 0, r = 0, t = 0;
    computeResolutionGateRect(vpW, vpH, gateAspect, filmFit, overscan, l, b, r, t);

    const double w = r - l;
    const double h = t - b;
    if (w <= 0.0 || h <= 0.0) return;

    // thirds lines inside the gate
    const double x1 = l + w / 3.0;
    const double x2 = l + w * 2.0 / 3.0;
    const double y1 = b + h / 3.0;
    const double y2 = b + h * 2.0 / 3.0;

    drawManager.beginDrawable();

    // fixed style (v0.2)
    drawManager.setColor(MColor(0.2f, 1.0f, 0.2f, 1.0f));
    drawManager.setLineWidth(1.0f);

    // Draw thirds
    drawManager.line2d(MPoint(x1, b), MPoint(x1, t));
    drawManager.line2d(MPoint(x2, b), MPoint(x2, t));
    drawManager.line2d(MPoint(l, y1), MPoint(r, y1));
    drawManager.line2d(MPoint(l, y2), MPoint(r, y2));

    // Label
    drawManager.setFontSize(12);
    drawManager.text2d(
        MPoint(l + 8.0, t - 18.0),
        "ao_viewport_guide v0.2 (Resolution Gate)",
        MUIDrawManager::kLeft
    );

    drawManager.endDrawable();
}
