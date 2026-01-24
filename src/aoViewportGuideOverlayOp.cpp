// aoViewportGuideOverlayOp.cpp
// Viewport 2.0 HUD-stuck overlay operation (Resolution Gate + thirds)
// - Draw only in "camera view" (non-startup camera, perspective)
// - Gate aspect: defaultResolution (width/height/pixelAspect)
//
// IMPORTANT:
// - Draw ONLY with MUIDrawManager::line2d()/text2d() (screen-space).
// - Keep z constant within [0,1] for true HUD-stuck overlay.
// - beginDrawInXray() to ensure it draws on top (no depth test).

#include <maya/MString.h>
#include <maya/MStatus.h>
#include <maya/MGlobal.h>
#include <maya/MPoint.h>
#include <maya/MColor.h>
#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>
#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>

#include <maya/MViewport2Renderer.h>
#include <maya/MDrawContext.h>
#include <maya/MFrameContext.h>
#include <maya/MUserRenderOperation.h>
#include <maya/MUIDrawManager.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    // -----------------------------
    // Config (DG network node)
    // -----------------------------
    static const char* kConfigNodeName = "ao_viewport_guide";

    static bool objExists(const MString& name)
    {
        int exists = 0;
        MGlobal::executeCommand(("objExists \"" + name + "\""), exists);
        return exists != 0;
    }

    static void ensureConfigNode()
    {
        if (objExists(kConfigNodeName))
            return;

        MString cmd;
        cmd += "createNode network -n \""; cmd += kConfigNodeName; cmd += "\";\n";

        cmd += "addAttr -ln \"enable\" -at bool -dv 1 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"debugText\" -at bool -dv 1 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"opacity\" -at double -min 0 -max 1 -dv 1 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"lineWidth\" -at double -min 0.5 -max 10 -dv 2 \""; cmd += kConfigNodeName; cmd += "\";\n";

        // Gate (magenta)
        cmd += "addAttr -ln \"gateColor\" -at double3 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"gateR\" -at double -min 0 -max 1 -dv 1.0 -p \"gateColor\" \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"gateG\" -at double -min 0 -max 1 -dv 0.0 -p \"gateColor\" \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"gateB\" -at double -min 0 -max 1 -dv 1.0 -p \"gateColor\" \""; cmd += kConfigNodeName; cmd += "\";\n";

        // Grid (green)
        cmd += "addAttr -ln \"gridColor\" -at double3 \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"gridR\" -at double -min 0 -max 1 -dv 0.0 -p \"gridColor\" \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"gridG\" -at double -min 0 -max 1 -dv 1.0 -p \"gridColor\" \""; cmd += kConfigNodeName; cmd += "\";\n";
        cmd += "addAttr -ln \"gridB\" -at double -min 0 -max 1 -dv 0.7 -p \"gridColor\" \""; cmd += kConfigNodeName; cmd += "\";\n";

        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".enable\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".debugText\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".opacity\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".lineWidth\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".gateColor\";\n";
        cmd += "setAttr -e -keyable true \""; cmd += kConfigNodeName; cmd += ".gridColor\";\n";

        MGlobal::executeCommand(cmd);
        MGlobal::displayInfo("[ao_viewport_guide] created config node: " + MString(kConfigNodeName));
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

    static MColor getPlugColor3(const MObject& node, const char* attrName, const MColor& defValue)
    {
        MFnDependencyNode fn(node);
        MPlug p = fn.findPlug(attrName, true);
        if (p.isNull()) return defValue;

        double r = defValue.r, g = defValue.g, b = defValue.b;
        if (p.numChildren() >= 3)
        {
            p.child(0).getValue(r);
            p.child(1).getValue(g);
            p.child(2).getValue(b);
        }
        return MColor((float)r, (float)g, (float)b, defValue.a);
    }

    static double safeDiv(double a, double b, double fallback = 0.0)
    {
        if (std::abs(b) < 1e-9) return fallback;
        return a / b;
    }

    static bool getDefaultResolution(double& outW, double& outH, double& outPixelAspect, double& outDeviceAspect)
    {
        MObject resNode;
        if (!getNodeByName("defaultResolution", resNode)) return false;

        outW = getPlugDouble(resNode, "width", 1920.0);
        outH = getPlugDouble(resNode, "height", 1080.0);
        outPixelAspect = getPlugDouble(resNode, "pixelAspect", 1.0);

        outDeviceAspect = getPlugDouble(
            resNode, "deviceAspectRatio",
            (outH > 0.0) ? (outW * outPixelAspect / outH) : 1.777777);

        return true;
    }

    // ★ カメラが “startupCamera” (persp/top/front/side) かどうか
    static bool isStartupCamera(const MDagPath& camPath)
    {
        if (!camPath.isValid()) return false;
        MFnDependencyNode fn(camPath.node());
        MPlug p = fn.findPlug("startupCamera", true);
        if (p.isNull()) return false;

        bool v = false;
        p.getValue(v);
        return v;
    }

    struct Rect
    {
        double l=0, b=0, r=0, t=0; // screen-space pixels
    };

    static Rect computeGateRectCentered(
        int vpX, int vpY, int vpW, int vpH,
        double gateAR,
        double overscan
    )
    {
        overscan = std::max(0.0001, overscan);

        const double viewAR = safeDiv((double)vpW, (double)vpH, 1.0);
        const double cx = (double)vpX + (double)vpW * 0.5;
        const double cy = (double)vpY + (double)vpH * 0.5;

        double gw=0, gh=0;
        if (viewAR >= gateAR)
        {
            gh = (double)vpH / overscan;
            gw = gh * gateAR;
        }
        else
        {
            gw = (double)vpW / overscan;
            gh = gw / gateAR;
        }

        Rect rc;
        rc.l = cx - gw * 0.5;
        rc.r = cx + gw * 0.5;
        rc.b = cy - gh * 0.5;
        rc.t = cy + gh * 0.5;
        return rc;
    }

    static void drawRect2D(MHWRender::MUIDrawManager& dm, const Rect& rc, double z)
    {
        dm.line2d(MPoint(rc.l, rc.b, z), MPoint(rc.r, rc.b, z));
        dm.line2d(MPoint(rc.r, rc.b, z), MPoint(rc.r, rc.t, z));
        dm.line2d(MPoint(rc.r, rc.t, z), MPoint(rc.l, rc.t, z));
        dm.line2d(MPoint(rc.l, rc.t, z), MPoint(rc.l, rc.b, z));
    }

    static void drawThirds(MHWRender::MUIDrawManager& dm, const Rect& rc, double z)
    {
        const double w = rc.r - rc.l;
        const double h = rc.t - rc.b;

        const double x1 = rc.l + w / 3.0;
        const double x2 = rc.l + w * 2.0 / 3.0;
        dm.line2d(MPoint(x1, rc.b, z), MPoint(x1, rc.t, z));
        dm.line2d(MPoint(x2, rc.b, z), MPoint(x2, rc.t, z));

        const double y1 = rc.b + h / 3.0;
        const double y2 = rc.b + h * 2.0 / 3.0;
        dm.line2d(MPoint(rc.l, y1, z), MPoint(rc.r, y1, z));
        dm.line2d(MPoint(rc.l, y2, z), MPoint(rc.r, y2, z));
    }
}

// ============================================================================
// AOViewportGuideOverlayOp
// ============================================================================
class AOViewportGuideOverlayOp : public MHWRender::MUserRenderOperation
{
public:
    explicit AOViewportGuideOverlayOp(const MString& name)
        : MHWRender::MUserRenderOperation(name)
    {}

    ~AOViewportGuideOverlayOp() override = default;

    MStatus execute(const MHWRender::MDrawContext& drawContext) override
    {
        ensureConfigNode();
        fConfigNode = MObject::kNullObj;
        getNodeByName(kConfigNodeName, fConfigNode);

        fShouldDraw = false;

        // camera path (this is per-panel)
        fOverscan = 1.0;
        MDagPath camPath = drawContext.getCurrentCameraPath();
        if (!camPath.isValid())
            return MS::kSuccess;

        // filter: camera view only (exclude startup cameras)
        if (isStartupCamera(camPath))
            return MS::kSuccess;

        // filter: exclude orthographic (top/front/side etc.)
        {
            MStatus s;
            MFnCamera camFn(camPath, &s);
            if (s != MS::kSuccess)
                return MS::kSuccess;

            if (camFn.isOrtho())
                return MS::kSuccess;

            fOverscan = camFn.overscan();
        }

        // viewport dims
        int x=0, y=0, w=0, h=0;
        drawContext.getViewportDimensions(x, y, w, h);
        fVpX=x; fVpY=y; fVpW=std::max(1,w); fVpH=std::max(1,h);

        // gate aspect from defaultResolution
        fResW=1920.0; fResH=1080.0; fPixelAspect=1.0; fDeviceAspect=1.777777;
        getDefaultResolution(fResW, fResH, fPixelAspect, fDeviceAspect);
        fGateAR = (fResH > 0.0) ? (fResW * fPixelAspect / fResH) : fDeviceAspect;

        // compute gate rect
        fGateRect = computeGateRectCentered(fVpX, fVpY, fVpW, fVpH, fGateAR, fOverscan);

        fShouldDraw = true;
        return MS::kSuccess;
    }

    bool hasUIDrawables() const override { return true; }

    void addUIDrawables(MHWRender::MUIDrawManager& dm,
                        const MHWRender::MFrameContext& /*frameContext*/) override
    {
        if (!fShouldDraw) return;
        if (fConfigNode.isNull()) return;

        const bool enable = getPlugBool(fConfigNode, "enable", true);
        if (!enable) return;

        const bool debugText = getPlugBool(fConfigNode, "debugText", true);
        const double opacity = std::clamp(getPlugDouble(fConfigNode, "opacity", 1.0), 0.0, 1.0);
        const double lineWidth = std::clamp(getPlugDouble(fConfigNode, "lineWidth", 2.0), 0.5, 10.0);

        MColor gateCol = getPlugColor3(fConfigNode, "gateColor", MColor(1,0,1,1));
        MColor gridCol = getPlugColor3(fConfigNode, "gridColor", MColor(0,1,0.7f,1));
        gateCol.a = (float)opacity;
        gridCol.a = (float)opacity;

        dm.beginDrawable(MHWRender::MUIDrawManager::kNonSelectable, 0);
        dm.setLineWidth((float)lineWidth);

        // Always on top of scene
        dm.beginDrawInXray();

        // HUD-stuck: z must be in [0,1] and constant for all 2D segments
        const double z = 0.0;

        // Gate rect
        dm.setColor(gateCol);
        drawRect2D(dm, fGateRect, z);

        // Thirds
        dm.setColor(gridCol);
        drawThirds(dm, fGateRect, z);

        // Debug text
        if (debugText)
        {
            dm.setColor(MColor(1,1,0,1));
            std::ostringstream ss;
            ss.setf(std::ios::fixed);
            ss.precision(4);
            ss << "vp(" << fVpX << "," << fVpY << ") size(" << fVpW << "x" << fVpH
               << ") gateAR=" << fGateAR << " overscan=" << fOverscan;
            const MString line(ss.str().c_str());

            const double tx = (double)fVpX + 12.0;
            const double ty = (double)fVpY + (double)fVpH - 20.0;
            dm.text2d(MPoint(tx, ty, z), line);
        }

        dm.endDrawInXray();
        dm.endDrawable();
    }

private:
    MObject fConfigNode;
    bool   fShouldDraw = false;

    int    fVpX=0, fVpY=0, fVpW=1, fVpH=1;
    double fOverscan=1.0;

    double fResW=1920.0, fResH=1080.0;
    double fPixelAspect=1.0;
    double fDeviceAspect=1.777777;
    double fGateAR=1.777777;

    Rect   fGateRect;
};

// ---------------------------------------------------------------------------
// Factory (optional)
// ---------------------------------------------------------------------------
AOViewportGuideOverlayOp* AOViewportGuideOverlayOp_Create(const MString& name)
{
    return new AOViewportGuideOverlayOp(name);
}
