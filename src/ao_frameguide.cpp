// ao_frameguide.cpp
// Maya2025 VP2 - "Cause-Finding" Debug Overlay for Resolution Gate
//
// What it draws:
// - Viewport pixel rect (RED)
// - Gate rect computed from defaultResolution aspect + camera filmFit/overscan (MAGENTA)
// - Gate rect with "temporary filmFitOffset applied" (CYAN)  ※offsetが原因か切り分け
// - Thirds grid inside computed gate (GREEN, configurable)
// - Debug text (YELLOW): vp dims, res, aspect, filmFit, overscan, filmFitOffset, gate rect etc.
// - Corner markers (WHITE) to confirm origin & vpX/vpY handling.
//
// Config node (auto-created if missing): network "ao_frameguide"
//   enable(bool) opacity(double) thickness(double) color(double3) debug(bool) applyFitOffset(bool)
//
// Build requirement:
// - Add DevKit include & lib:
//   E:\mayadev\devkitBase\include
//   E:\mayadev\devkitBase\lib
//
// NOTE:
// - filmFitOffset's real mapping is tricky; this debug version applies a *temporary heuristic*
//   (offset * gateSize) to visualize whether offset is part of the observed drift.
//   If CYAN rect matches Maya gate while MAGENTA doesn't, offset handling is the culprit.

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>
#include <maya/MStatus.h>
#include <maya/MPoint.h>
#include <maya/MColor.h>
#include <maya/MSelectionList.h>
#include <maya/MObject.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>

// VP2 / RenderOverride
#include <maya/MRenderer.h>
#include <maya/MRenderOverride.h>
#include <maya/MRenderOperation.h>
#include <maya/MUserRenderOperation.h>
#include <maya/MSceneRender.h>
#include <maya/MClearOperation.h>
#include <maya/MHUDRender.h>
#include <maya/MPresentTarget.h>
#include <maya/MFrameContext.h>
#include <maya/MUIDrawManager.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    static const char* kConfigNodeName = "ao_frameguide";

    static const MString kOverrideName("ao_frameguide_override");
    static const MString kOverrideUIName("ao_frameguide (debug)");

    // ---------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------
    static double clampd(double v, double a, double b)
    {
        return std::max(a, std::min(b, v));
    }

    static double safeDiv(double a, double b, double fallback = 0.0)
    {
        if (std::abs(b) < 1e-9) return fallback;
        return a / b;
    }

    static bool objExists(const MString& name)
    {
        int exists = 0;
        MGlobal::executeCommand(("objExists \"" + name + "\""), exists);
        return exists != 0;
    }

    static bool getNodeByName(const MString& name, MObject& outObj)
    {
        MSelectionList sl;
        if (sl.add(name) != MS::kSuccess) return false;
        if (sl.getDependNode(0, outObj) != MS::kSuccess) return false;
        return true;
    }

    static double getPlugDouble(const MObject& node, const char* attrName, double defValue)
    {
        MFnDependencyNode fn(node);
        MPlug p = fn.findPlug(attrName, true);
        if (p.isNull()) return defValue;
        double v = defValue;
        p.getValue(v);
        return v;
    }

    static bool getPlugBool(const MObject& node, const char* attrName, bool defValue)
    {
        MFnDependencyNode fn(node);
        MPlug p = fn.findPlug(attrName, true);
        if (p.isNull()) return defValue;
        bool v = defValue;
        p.getValue(v);
        return v;
    }

    static MColor getPlugColor(const MObject& node, const char* attrName, const MColor& defValue)
    {
        MFnDependencyNode fn(node);
        MPlug p = fn.findPlug(attrName, true);
        if (p.isNull()) return defValue;

        MColor c = defValue;
        // double3
        double r = defValue.r, g = defValue.g, b = defValue.b;
        p.child(0).getValue(r);
        p.child(1).getValue(g);
        p.child(2).getValue(b);
        c.r = (float)r; c.g = (float)g; c.b = (float)b;
        return c;
    }

    static void ensureConfigNode()
    {
        if (objExists(kConfigNodeName)) return;

        MString cmd;
        cmd += "createNode network -n \""; cmd += kConfigNodeName; cmd += "\";\n";

        cmd += "addAttr -ln \"enable\" -at bool -dv 1 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"opacity\" -at double -min 0 -max 1 -dv 1 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"thickness\" -at double -min 0.5 -max 20 -dv 2 \""; cmd += kConfigNodeName; cmd += "\";\n";

        cmd += "addAttr -ln \"color\" -at double3 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"colorR\" -at double -min 0 -max 1 -dv 0.0 -p \"color\" \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"colorG\" -at double -min 0 -max 1 -dv 1.0 -p \"color\" \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"colorB\" -at double -min 0 -max 1 -dv 0.7 -p \"color\" \""; cmd += kConfigNodeName; cmd += "\";\n";

        cmd += "addAttr -ln \"debug\" -at bool -dv 1 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"applyFitOffset\" -at bool -dv 0 \""; cmd += kConfigNodeName; cmd += "\";\n";

        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".enable\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".opacity\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".thickness\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".color\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".debug\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".applyFitOffset\";\n";

        MGlobal::executeCommand(cmd);
        MGlobal::displayInfo("[ao_frameguide] created config node: " + MString(kConfigNodeName));
    }

    static bool getDefaultResolution(double& outW, double& outH, double& outPixelAspect, double& outDeviceAspect)
    {
        MObject resNode;
        if (!getNodeByName("defaultResolution", resNode)) return false;

        outW = getPlugDouble(resNode, "width", 1920.0);
        outH = getPlugDouble(resNode, "height", 1080.0);
        outPixelAspect = getPlugDouble(resNode, "pixelAspect", 1.0);
        outDeviceAspect = getPlugDouble(resNode, "deviceAspectRatio", safeDiv(outW * outPixelAspect, std::max(1.0, outH), 1.777777));
        return true;
    }

    struct RectD
    {
        double l = 0.0;
        double b = 0.0;
        double w = 0.0;
        double h = 0.0;
    };

    static const char* filmFitToStr(MFnCamera::FilmFit fit)
    {
        switch (fit)
        {
        case MFnCamera::kFillFilmFit:       return "Fill";
        case MFnCamera::kHorizontalFilmFit: return "Horizontal";
        case MFnCamera::kVerticalFilmFit:   return "Vertical";
        case MFnCamera::kOverscanFilmFit:   return "Overscan";
        default:                            return "Unknown";
        }
    }

    // Compute gate rect inside viewport given:
    // - viewport rect (x,y,w,h)
    // - gate aspect ratio (defaultResolution-based)
    // - camera filmFit + overscan
    //
    // NOTE: This produces "theoretical gate rect".
    // Later you’ll align it with what Maya displays by refining:
    // - where vpX/vpY come from
    // - DPI / devicePixelRatio issues (floating window)
    // - exact mapping of filmFitOffset
    static RectD computeGateRectFilmFit(
        int vpX, int vpY, int vpW, int vpH,
        double gateAR,
        double overscan,
        MFnCamera::FilmFit filmFit
    )
    {
        overscan = std::max(0.0001, overscan);

        const double viewAR = safeDiv((double)vpW, (double)vpH, 1.0);
        const double cx = (double)vpX + (double)vpW * 0.5;
        const double cy = (double)vpY + (double)vpH * 0.5;

        RectD r;
        double gateW = 0.0, gateH = 0.0;

        // Decide how gate fits based on filmFit
        // - Overscan: show whole gate within viewport (letter/pillar box)
        // - Fill: fill viewport (crop the gate)
        // - Horizontal: fit width
        // - Vertical: fit height
        switch (filmFit)
        {
        case MFnCamera::kHorizontalFilmFit:
            gateW = (double)vpW / overscan;
            gateH = safeDiv(gateW, gateAR, (double)vpH / overscan);
            break;

        case MFnCamera::kVerticalFilmFit:
            gateH = (double)vpH / overscan;
            gateW = gateH * gateAR;
            break;

        case MFnCamera::kFillFilmFit:
            // Fill viewport: gate becomes larger in the dimension that causes cropping
            if (viewAR >= gateAR)
            {
                // viewport wider -> fit width => height becomes larger (crop top/bottom)
                gateW = (double)vpW / overscan;
                gateH = safeDiv(gateW, gateAR, (double)vpH / overscan);
            }
            else
            {
                // viewport taller -> fit height => width becomes larger (crop left/right)
                gateH = (double)vpH / overscan;
                gateW = gateH * gateAR;
            }
            break;

        case MFnCamera::kOverscanFilmFit:
        default:
            // Show whole gate within viewport
            if (viewAR >= gateAR)
            {
                // viewport wider -> fit height
                gateH = (double)vpH / overscan;
                gateW = gateH * gateAR;
            }
            else
            {
                // viewport taller -> fit width
                gateW = (double)vpW / overscan;
                gateH = safeDiv(gateW, gateAR, (double)vpH / overscan);
            }
            break;
        }

        r.w = gateW;
        r.h = gateH;
        r.l = cx - gateW * 0.5;
        r.b = cy - gateH * 0.5;
        return r;
    }

    // Temporary heuristic offset apply (for diagnosis)
    // Maya's filmFitOffset is not simply "ratio * pixels" in all cases.
    // This is only to see if offset explains the observed drift.
    static RectD applyFitOffsetHeuristic(const RectD& base, double filmFitOffset, MFnCamera::FilmFit filmFit)
    {
        RectD r = base;

        // heuristic: offset shifts along the "fit direction"
        // Horizontal: X shift, Vertical: Y shift, others: X shift
        if (filmFit == MFnCamera::kVerticalFilmFit)
        {
            // Y shift by fraction of height
            const double dy = filmFitOffset * base.h;
            r.b += dy;
        }
        else
        {
            // X shift by fraction of width
            const double dx = filmFitOffset * base.w;
            r.l += dx;
        }
        return r;
    }

} // anon namespace

// ============================================================================
// User operation that draws the overlay using MUIDrawManager
// ============================================================================
class AoFrameGuideUserOp : public MHWRender::MUserRenderOperation
{
public:
    AoFrameGuideUserOp(const MString& name)
        : MHWRender::MUserRenderOperation(name)
    {}

    ~AoFrameGuideUserOp() override = default;

    MStatus execute(const MHWRender::MDrawContext& /*drawContext*/) override
    {
        // No heavy GPU calls here. We do everything in addUIDrawables().
        ensureConfigNode();
        getNodeByName(kConfigNodeName, fConfigNode);
        return MS::kSuccess;
    }

    bool hasUIDrawables() const override
    {
        return true;
    }

    void addUIDrawables(MHWRender::MUIDrawManager& dm, const MHWRender::MFrameContext& frameContext) override
    {
        if (fConfigNode.isNull()) return;

        const bool enable = getPlugBool(fConfigNode, "enable", true);
        if (!enable) return;

        const bool debug = getPlugBool(fConfigNode, "debug", true);
        const bool applyFitOffset = getPlugBool(fConfigNode, "applyFitOffset", false);

        const double opacity = clampd(getPlugDouble(fConfigNode, "opacity", 1.0), 0.0, 1.0);
        const double thickness = clampd(getPlugDouble(fConfigNode, "thickness", 2.0), 0.5, 20.0);

        MColor guideColor = getPlugColor(fConfigNode, "color", MColor(0.0f, 1.0f, 0.7f, 1.0f));
        guideColor.a = (float)opacity;

        // --- viewport dims (IMPORTANT) ---
        int vpX = 0, vpY = 0, vpW = 1, vpH = 1;
        frameContext.getViewportDimensions(vpX, vpY, vpW, vpH);
        const double viewAR = safeDiv((double)vpW, (double)vpH, 1.0);

        // --- current camera ---
        bool hasCamera = false;
        double overscan = 1.0;
        double filmFitOffset = 0.0;
        MFnCamera::FilmFit filmFit = MFnCamera::kOverscanFilmFit;

        MStatus camStat;
        MDagPath camPath = frameContext.getCurrentCameraPath(&camStat);
        if (camStat == MS::kSuccess && camPath.isValid())
        {
            MStatus s;
            MFnCamera camFn(camPath, &s);
            if (s == MS::kSuccess)
            {
                hasCamera = true;
                overscan = camFn.overscan();
                filmFit = camFn.filmFit();
                filmFitOffset = camFn.filmFitOffset();
            }
        }

        // --- defaultResolution ---
        double resW = 1920.0, resH = 1080.0, pixelAspect = 1.0, deviceAspect = 1.777777;
        getDefaultResolution(resW, resH, pixelAspect, deviceAspect);

        const double gateAR = safeDiv(resW * pixelAspect, std::max(1.0, resH), deviceAspect);

        // --- compute gate rect ---
        RectD gateRect = computeGateRectFilmFit(vpX, vpY, vpW, vpH, gateAR, overscan, filmFit);
        RectD gateRectOffset = applyFitOffsetHeuristic(gateRect, filmFitOffset, filmFit);

        // --- draw ---
        dm.beginDrawable();

        dm.setDepthPriority(5);         // draw on top
        dm.setLineWidth((float)thickness);

        // 1) viewport border (RED)
        if (debug)
        {
            dm.setColor(MColor(1, 0, 0, 1));
            const double L = (double)vpX;
            const double B = (double)vpY;
            const double R = (double)vpX + (double)vpW;
            const double T = (double)vpY + (double)vpH;

            dm.line2d(MPoint(L, B), MPoint(R, B));
            dm.line2d(MPoint(R, B), MPoint(R, T));
            dm.line2d(MPoint(R, T), MPoint(L, T));
            dm.line2d(MPoint(L, T), MPoint(L, B));

            // corner markers + labels (WHITE)
            dm.setColor(MColor(1, 1, 1, 1));
            dm.point2d(MPoint(L, B), 6.0f);
            dm.point2d(MPoint(R, B), 6.0f);
            dm.point2d(MPoint(L, T), 6.0f);
            dm.point2d(MPoint(R, T), 6.0f);

            dm.text2d(MPoint(L + 8,  B + 8),  "VP(L,B)");
            dm.text2d(MPoint(R - 70, B + 8),  "VP(R,B)");
            dm.text2d(MPoint(L + 8,  T - 18), "VP(L,T)");
            dm.text2d(MPoint(R - 70, T - 18), "VP(R,T)");
        }

        // 2) gate rect (MAGENTA)
        {
            dm.setColor(MColor(1, 0, 1, 1));
            const double L = gateRect.l;
            const double B = gateRect.b;
            const double R = gateRect.l + gateRect.w;
            const double T = gateRect.b + gateRect.h;

            dm.line2d(MPoint(L, B), MPoint(R, B));
            dm.line2d(MPoint(R, B), MPoint(R, T));
            dm.line2d(MPoint(R, T), MPoint(L, T));
            dm.line2d(MPoint(L, T), MPoint(L, B));
        }

        // 3) gate rect with heuristic offset (CYAN) - optional
        if (debug)
        {
            dm.setColor(MColor(0, 1, 1, 1));
            RectD r = applyFitOffset ? gateRectOffset : gateRectOffset; // always draw for diagnosis
            const double L = r.l;
            const double B = r.b;
            const double R = r.l + r.w;
            const double T = r.b + r.h;

            dm.line2d(MPoint(L, B), MPoint(R, B));
            dm.line2d(MPoint(R, B), MPoint(R, T));
            dm.line2d(MPoint(R, T), MPoint(L, T));
            dm.line2d(MPoint(L, T), MPoint(L, B));
        }

        // 4) thirds grid inside MAGENTA rect (GREEN-ish configurable)
        {
            dm.setColor(guideColor);

            const double L = gateRect.l;
            const double B = gateRect.b;
            const double W = gateRect.w;
            const double H = gateRect.h;

            const double x1 = L + W / 3.0;
            const double x2 = L + W * 2.0 / 3.0;
            const double y1 = B + H / 3.0;
            const double y2 = B + H * 2.0 / 3.0;

            dm.line2d(MPoint(x1, B),     MPoint(x1, B + H));
            dm.line2d(MPoint(x2, B),     MPoint(x2, B + H));
            dm.line2d(MPoint(L,  y1),    MPoint(L + W, y1));
            dm.line2d(MPoint(L,  y2),    MPoint(L + W, y2));
        }

        // 5) debug text (YELLOW)
        if (debug)
        {
            dm.setColor(MColor(1, 1, 0, 1));

            std::ostringstream ss;
            ss.setf(std::ios::fixed);
            ss.precision(4);

            ss << "vp=(" << vpX << "," << vpY << ") " << vpW << "x" << vpH
               << " viewAR=" << viewAR;
            MString line1(ss.str().c_str());

            ss.str(""); ss.clear();
            ss << "res=" << resW << "x" << resH
               << " pixelAspect=" << pixelAspect
               << " deviceAspect=" << deviceAspect
               << " gateAR=" << gateAR;
            MString line2(ss.str().c_str());

            ss.str(""); ss.clear();
            ss << "camera=" << (hasCamera ? "OK" : "NONE")
               << " filmFit=" << filmFitToStr(filmFit)
               << " overscan=" << overscan
               << " filmFitOffset=" << filmFitOffset
               << " applyFitOffset=" << (applyFitOffset ? "ON" : "OFF");
            MString line3(ss.str().c_str());

            ss.str(""); ss.clear();
            ss << "gate(MAGENTA) L,B,W,H = "
               << gateRect.l << ", " << gateRect.b << ", " << gateRect.w << ", " << gateRect.h;
            MString line4(ss.str().c_str());

            ss.str(""); ss.clear();
            ss << "gate(CYAN, heuristic offset) L,B,W,H = "
               << gateRectOffset.l << ", " << gateRectOffset.b << ", " << gateRectOffset.w << ", " << gateRectOffset.h;
            MString line5(ss.str().c_str());

            const double tx = (double)vpX + 12.0;
            const double ty = (double)vpY + (double)vpH - 20.0;

            dm.text2d(MPoint(tx, ty),         line1);
            dm.text2d(MPoint(tx, ty - 18.0),  line2);
            dm.text2d(MPoint(tx, ty - 36.0),  line3);
            dm.text2d(MPoint(tx, ty - 54.0),  line4);
            dm.text2d(MPoint(tx, ty - 72.0),  line5);
        }

        dm.endDrawable();
    }

private:
    MObject fConfigNode;
};

// ============================================================================
// RenderOverride
// ============================================================================
class AoFrameGuideOverride : public MHWRender::MRenderOverride
{
public:
    AoFrameGuideOverride(const MString& name)
        : MHWRender::MRenderOverride(name)
    {
        fClearOp   = new MHWRender::MClearOperation("ao_fg_clear");
        fSceneOp   = new MHWRender::MSceneRender("ao_fg_scene");
        fUserOp    = new AoFrameGuideUserOp("ao_fg_user");
        fHudOp     = new MHWRender::MHUDRender("ao_fg_hud");
        fPresentOp = new MHWRender::MPresentTarget("ao_fg_present");

        // Debug: make sure we can see "override is running"
        fClearOp->setClearColor(MColor(0.15f, 0.15f, 0.15f, 1.0f));
        fClearOp->setMask((unsigned int)MHWRender::MClearOperation::kClearAll);

        // Operation order
        fOps[0] = fClearOp;
        fOps[1] = fSceneOp;    // show Maya scene
        fOps[2] = fUserOp;     // our overlay
        fOps[3] = fHudOp;      // keep manipulators/HUD (optional but useful)
        fOps[4] = fPresentOp;  // MUST present, or black output
        fOpCount = 5;
    }

    ~AoFrameGuideOverride() override
    {
        delete fClearOp;   fClearOp = nullptr;
        delete fSceneOp;   fSceneOp = nullptr;
        delete fUserOp;    fUserOp = nullptr;
        delete fHudOp;     fHudOp = nullptr;
        delete fPresentOp; fPresentOp = nullptr;
    }

    MString uiName() const override
    {
        return kOverrideUIName;
    }

    MHWRender::DrawAPI supportedDrawAPIs() const override
    {
        // Keep permissive
        return (MHWRender::kOpenGL | MHWRender::kDirectX11);
    }

    MStatus setup(const MString& destination) override
    {
        fDestination = destination;
        ensureConfigNode();
        return MS::kSuccess;
    }

    MStatus cleanup() override
    {
        return MS::kSuccess;
    }

    bool startOperationIterator() override
    {
        fOpIndex = 0;
        return true;
    }

    MHWRender::MRenderOperation* renderOperation() override
    {
        if (fOpIndex < fOpCount) return fOps[fOpIndex];
        return nullptr;
    }

    bool nextRenderOperation() override
    {
        ++fOpIndex;
        return (fOpIndex < fOpCount);
    }

private:
    MString fDestination;

    unsigned int fOpIndex = 0;
    unsigned int fOpCount = 0;

    MHWRender::MClearOperation*  fClearOp   = nullptr;
    MHWRender::MSceneRender*     fSceneOp   = nullptr;
    AoFrameGuideUserOp*          fUserOp    = nullptr;
    MHWRender::MHUDRender*       fHudOp     = nullptr;
    MHWRender::MPresentTarget*   fPresentOp = nullptr;

    MHWRender::MRenderOperation* fOps[8]{};
};

// ============================================================================
// Plugin entry
// ============================================================================
static AoFrameGuideOverride* gOverride = nullptr;

MStatus initializePlugin(MObject obj)
{
    MFnPlugin plugin(obj, "ao_frameguide", "0.0.1", "Any");

    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
    if (!renderer)
    {
        MGlobal::displayError("[ao_frameguide] VP2 renderer not available.");
        return MS::kFailure;
    }

    if (!gOverride)
        gOverride = new AoFrameGuideOverride(kOverrideName);

    MStatus s = renderer->registerOverride(gOverride);
    if (s != MS::kSuccess)
    {
        MGlobal::displayError("[ao_frameguide] registerOverride failed.");
        delete gOverride;
        gOverride = nullptr;
        return s;
    }

    MGlobal::displayInfo("[ao_frameguide] override registered: " + kOverrideName);
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject /*obj*/)
{
    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
    if (renderer && gOverride)
    {
        renderer->deregisterOverride(gOverride);
        MGlobal::displayInfo("[ao_frameguide] override deregistered.");
    }

    delete gOverride;
    gOverride = nullptr;

    return MS::kSuccess;
}
