// aoViewportGuidePlugin.cpp (Maya 2025 / Windows)
// Viewport 2.0 Render Override: Scene -> HUD -> Present
// + Settings node: aoViewportGuideSettings (auto-create on plugin load)

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>
#include <maya/MStatus.h>

#include <maya/MViewport2Renderer.h>
#include <maya/MFrameContext.h>
#include <maya/MUIDrawManager.h>

#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>

#include <maya/MDoubleArray.h>
#include <maya/MColor.h>
#include <maya/MPoint.h>

#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>

#include <maya/MPxNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MItDependencyNodes.h>

#include <algorithm>
#include <cstdio>

// ------------------------------------------------------------
// Utility
// ------------------------------------------------------------
static inline float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

namespace {

// ------------------------------------------------------------
// IDs / Names
// ------------------------------------------------------------
constexpr const char* kOverrideIdName = "ao_viewport_guide";
constexpr const char* kOverrideUiName = "AO Viewport Guide";

constexpr const char* kSettingsNodeTypeName = "aoViewportGuideSettings";
static const MTypeId  kSettingsNodeTypeId(0x0013A0F2);   // 衝突しない固定値に（必要なら後で変更）

constexpr const char* kAutoNodeName = "aoViewportGuideSettings1";
static MString gSettingsNodeName;

// ------------------------------------------------------------
// Defaults
// ------------------------------------------------------------
constexpr bool  kDefaultEnable   = true;
constexpr float kDefaultOpacity  = 0.85f;
constexpr float kDefaultThickPx  = 1.5f;
const MColor    kDefaultColorRGB(0.20f, 1.00f, 0.20f, 1.0f);

constexpr int   kDefaultGuideType = 0; // Thirds

constexpr bool  kFollowResolutionGate = true;
constexpr bool  kDrawGateBorder       = false; // デバッグ用
constexpr bool  kShowDebugText        = false; // デバッグ用

// ------------------------------------------------------------
// Settings Node (MPxNode)
// ------------------------------------------------------------
class AoViewportGuideSettingsNode : public MPxNode
{
public:
    static void* creator() { return new AoViewportGuideSettingsNode(); }
    static MStatus initialize();

    static MObject aEnable;
    static MObject aOpacity;
    static MObject aColor;
    static MObject aThickness;
    static MObject aGuideType; // enum
};

MObject AoViewportGuideSettingsNode::aEnable;
MObject AoViewportGuideSettingsNode::aOpacity;
MObject AoViewportGuideSettingsNode::aColor;
MObject AoViewportGuideSettingsNode::aThickness;
MObject AoViewportGuideSettingsNode::aGuideType;

MStatus AoViewportGuideSettingsNode::initialize()
{
    MStatus s;

    MFnNumericAttribute nAttr;

    aEnable = nAttr.create("enable", "en", MFnNumericData::kBoolean, kDefaultEnable, &s);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aEnable);

    aOpacity = nAttr.create("opacity", "op", MFnNumericData::kFloat, kDefaultOpacity, &s);
    nAttr.setKeyable(true); nAttr.setMin(0.0); nAttr.setMax(1.0);
    nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aOpacity);

    aThickness = nAttr.create("thickness", "th", MFnNumericData::kFloat, kDefaultThickPx, &s);
    nAttr.setKeyable(true); nAttr.setMin(0.5); nAttr.setMax(10.0);
    nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aThickness);

    aColor = nAttr.createColor("color", "cl", &s);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    nAttr.setDefault(kDefaultColorRGB.r, kDefaultColorRGB.g, kDefaultColorRGB.b);
    addAttribute(aColor);

    MFnEnumAttribute eAttr;
    aGuideType = eAttr.create("guideType", "gt", kDefaultGuideType, &s);
    eAttr.addField("Thirds (3x3)", 0);
    eAttr.setKeyable(true);
    eAttr.setStorable(true);
    eAttr.setChannelBox(true);
    addAttribute(aGuideType);

    return MS::kSuccess;
}

// ------------------------------------------------------------
// Find/Create settings node
// ------------------------------------------------------------
static bool ensureSettingsNodeExists()
{
    if (gSettingsNodeName.length() > 0)
    {
        MSelectionList sl;
        if (sl.add(gSettingsNodeName) == MS::kSuccess)
        {
            MObject obj;
            if (sl.getDependNode(0, obj) == MS::kSuccess && !obj.isNull())
                return true;
        }
        gSettingsNodeName.clear();
    }

    // Search existing
    MItDependencyNodes it(MFn::kPluginDependNode);
    for (; !it.isDone(); it.next())
    {
        MObject obj = it.item();
        MFnDependencyNode fn(obj);
        if (fn.typeId() == kSettingsNodeTypeId)
        {
            gSettingsNodeName = fn.name();
            return true;
        }
    }

    // Create if missing
    {
        MString cmd;
        cmd += "if (!`objExists \"";
        cmd += kAutoNodeName;
        cmd += "\"`) { createNode ";
        cmd += kSettingsNodeTypeName;
        cmd += " -n \"";
        cmd += kAutoNodeName;
        cmd += "\"; }";

        MStatus s = MGlobal::executeCommand(cmd, false, false);
        if (s != MS::kSuccess)
            return false;
    }

    // Re-search
    MItDependencyNodes it2(MFn::kPluginDependNode);
    for (; !it2.isDone(); it2.next())
    {
        MObject obj = it2.item();
        MFnDependencyNode fn(obj);
        if (fn.typeId() == kSettingsNodeTypeId)
        {
            gSettingsNodeName = fn.name();
            return true;
        }
    }

    return false;
}

// ------------------------------------------------------------
// Read settings
// ------------------------------------------------------------
struct GuideSettings
{
    bool  enable   = kDefaultEnable;
    float opacity  = kDefaultOpacity;
    float thicknessPx = kDefaultThickPx;
    MColor colorRGB = kDefaultColorRGB;
    int   guideType = kDefaultGuideType;
};

static GuideSettings readGuideSettings()
{
    GuideSettings gs;

    ensureSettingsNodeExists();

    if (gSettingsNodeName.length() <= 0)
        return gs;

    MSelectionList sl;
    if (sl.add(gSettingsNodeName) != MS::kSuccess)
        return gs;

    MObject obj;
    if (sl.getDependNode(0, obj) != MS::kSuccess || obj.isNull())
        return gs;

    MFnDependencyNode fn(obj);
    if (fn.typeId() != kSettingsNodeTypeId)
        return gs;

    {
        MPlug p = fn.findPlug(AoViewportGuideSettingsNode::aEnable, true);
        if (!p.isNull()) gs.enable = p.asBool();
    }
    {
        MPlug p = fn.findPlug(AoViewportGuideSettingsNode::aOpacity, true);
        if (!p.isNull()) gs.opacity = clampf(p.asFloat(), 0.0f, 1.0f);
    }
    {
        MPlug p = fn.findPlug(AoViewportGuideSettingsNode::aThickness, true);
        if (!p.isNull()) gs.thicknessPx = clampf(p.asFloat(), 0.5f, 10.0f);
    }
    {
        MPlug p = fn.findPlug(AoViewportGuideSettingsNode::aColor, true);
        if (!p.isNull() && p.numChildren() >= 3)
        {
            gs.colorRGB = MColor(
                clampf(p.child(0).asFloat(), 0.0f, 1.0f),
                clampf(p.child(1).asFloat(), 0.0f, 1.0f),
                clampf(p.child(2).asFloat(), 0.0f, 1.0f),
                1.0f
            );
        }
    }
    {
        MPlug p = fn.findPlug(AoViewportGuideSettingsNode::aGuideType, true);
        if (!p.isNull()) gs.guideType = p.asInt();
    }

    return gs;
}

// ------------------------------------------------------------
// defaultResolution aspect = width / height
// ------------------------------------------------------------
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
    return true;
}

// ------------------------------------------------------------
// camera info: overscan / fitResolutionGate
// ------------------------------------------------------------
struct CameraGateParams
{
    double overscan = 1.0;
    bool   fitIsFill = false;          // false=Overscan(contain), true=Fill(cover)
    bool   hasFitResolutionGate = false;
    bool   ok = false;
};

static CameraGateParams getCameraGateParams(const MHWRender::MFrameContext& frameContext)
{
    CameraGateParams p;

    MStatus s;
    MDagPath camPath = frameContext.getCurrentCameraPath(&s);
    if (s != MS::kSuccess)
        return p;

    MStatus cs;
    MFnCamera camFn(camPath, &cs);
    if (cs != MS::kSuccess)
        return p;

    p.overscan = camFn.overscan();

    MFnDependencyNode camDep(camPath.node(), &cs);
    if (cs == MS::kSuccess)
    {
        MStatus ps;
        MPlug fitPlug = camDep.findPlug("fitResolutionGate", true, &ps);
        if (ps == MS::kSuccess && !fitPlug.isNull())
        {
            const int v = fitPlug.asInt();
            p.hasFitResolutionGate = true;
            p.fitIsFill = (v == 0);
        }
    }

    p.ok = true;
    return p;
}

// ------------------------------------------------------------
// SceneRender
// ------------------------------------------------------------
class AoSceneRender : public MHWRender::MSceneRender
{
public:
    AoSceneRender(const MString& name) : MHWRender::MSceneRender(name) {}
};

// ------------------------------------------------------------
// HUDRender (NOTE: MHUDRender itself is a HUD render operation) :contentReference[oaicite:1]{index=1}
// ------------------------------------------------------------
class AoGuideHUD : public MHWRender::MHUDRender
{
public:
    bool hasUIDrawables() const override { return true; }

    void addUIDrawables(MHWRender::MUIDrawManager& dm,
                        const MHWRender::MFrameContext& frameContext) override
    {
        const GuideSettings gs = readGuideSettings();
        if (!gs.enable) return;
        if (gs.guideType != 0) return; // Thirds only

        int vpX=0, vpY=0, vpW=0, vpH=0;
        frameContext.getViewportDimensions(vpX, vpY, vpW, vpH);
        if (vpW < 10 || vpH < 10) return;

        const double viewW = static_cast<double>(vpW);
        const double viewH = static_cast<double>(vpH);

        double renderAspect = viewW / viewH;
        if (kFollowResolutionGate)
        {
            double ar = 1.0;
            if (getDefaultResolutionAspect(ar))
                renderAspect = ar;
        }

        const CameraGateParams camP = getCameraGateParams(frameContext);
        const double viewAspect = viewW / viewH;

        double rectW = viewW;
        double rectH = viewH;

        const bool useFill = (camP.hasFitResolutionGate ? camP.fitIsFill : false);

        if (!useFill)
        {
            // Overscan(contain)
            if (viewAspect > renderAspect) { rectH = viewH; rectW = rectH * renderAspect; }
            else                           { rectW = viewW; rectH = rectW / renderAspect; }
        }
        else
        {
            // Fill(cover)
            if (viewAspect > renderAspect) { rectW = viewW; rectH = rectW / renderAspect; }
            else                           { rectH = viewH; rectW = rectH * renderAspect; }
        }

        const double overscan = (camP.ok && camP.overscan > 0.0001) ? camP.overscan : 1.0;
        rectW /= overscan;
        rectH /= overscan;

        const double rectX = (viewW - rectW) * 0.5;
        const double rectY = (viewH - rectH) * 0.5;

        const double left   = rectX;
        const double bottom = rectY;
        const double right  = rectX + rectW;
        const double top    = rectY + rectH;

        const double x1 = left + rectW / 3.0;
        const double x2 = left + rectW * 2.0 / 3.0;
        const double y1 = bottom + rectH / 3.0;
        const double y2 = bottom + rectH * 2.0 / 3.0;

        MColor col = gs.colorRGB;
        col.a = clampf(gs.opacity, 0.0f, 1.0f);

        dm.beginDrawable();
        dm.setLineWidth(gs.thicknessPx);
        dm.setColor(col);

        if (kDrawGateBorder)
        {
            dm.line2d(MPoint(left,  bottom), MPoint(right, bottom));
            dm.line2d(MPoint(right, bottom), MPoint(right, top));
            dm.line2d(MPoint(right, top),    MPoint(left,  top));
            dm.line2d(MPoint(left,  top),    MPoint(left,  bottom));
        }

        dm.line2d(MPoint(x1, bottom), MPoint(x1, top));
        dm.line2d(MPoint(x2, bottom), MPoint(x2, top));
        dm.line2d(MPoint(left, y1),   MPoint(right, y1));
        dm.line2d(MPoint(left, y2),   MPoint(right, y2));

        dm.endDrawable();

        if (kShowDebugText)
        {
            char buf[256];
            sprintf_s(buf, "node=%s op=%.2f th=%.2f",
                      (gSettingsNodeName.length() > 0 ? gSettingsNodeName.asChar() : "none"),
                      gs.opacity, gs.thicknessPx);

            dm.beginDrawable();
            dm.setColor(MColor(1.0f, 0.0f, 1.0f, 1.0f));
            dm.setFontSize(MHWRender::MUIDrawManager::kSmallFontSize);
            dm.text2d(MPoint(20.0, viewH - 30.0), buf);
            dm.endDrawable();
        }
    }
};

// ------------------------------------------------------------
// RenderOverride: Scene -> HUD -> Present
// ------------------------------------------------------------
class AoViewportGuideOverride : public MHWRender::MRenderOverride
{
public:
    AoViewportGuideOverride(const MString& name)
        : MHWRender::MRenderOverride(name)
    {
        mScene   = new AoSceneRender("ao_viewportGuide_scene");
        mHud     = new AoGuideHUD();
        mPresent = new MHWRender::MPresentTarget("ao_viewportGuide_present");

        mOps[0] = mScene;
        mOps[1] = mHud;     // ★ここがポイント：MHUDRender をそのままオペレーションとして返す
        mOps[2] = mPresent;
    }

    ~AoViewportGuideOverride() override
    {
        delete mScene;   mScene = nullptr;
        delete mHud;     mHud = nullptr;
        delete mPresent; mPresent = nullptr;
    }

    MString uiName() const override { return kOverrideUiName; }

    MHWRender::DrawAPI supportedDrawAPIs() const override
    {
        return MHWRender::kAllDevices;
    }

    MStatus setup(const MString&) override
    {
        ensureSettingsNodeExists();
        return MS::kSuccess;
    }

    MStatus cleanup() override { return MS::kSuccess; }

    bool startOperationIterator() override { mIndex = 0; return true; }

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

    AoSceneRender*             mScene   = nullptr;
    AoGuideHUD*                mHud     = nullptr;
    MHWRender::MPresentTarget* mPresent = nullptr;

    MHWRender::MRenderOperation* mOps[3]{};
};

static AoViewportGuideOverride* gOverride = nullptr;

} // namespace

// ------------------------------------------------------------
// Plugin entry points
// ------------------------------------------------------------
MStatus initializePlugin(MObject obj)
{
    MStatus stat;
    MFnPlugin plugin(obj, "ao", "0.1.0", "Any", &stat);
    if (stat != MS::kSuccess) return stat;

    stat = plugin.registerNode(
        kSettingsNodeTypeName,
        kSettingsNodeTypeId,
        AoViewportGuideSettingsNode::creator,
        AoViewportGuideSettingsNode::initialize,
        MPxNode::kDependNode
    );
    if (stat != MS::kSuccess)
    {
        MGlobal::displayError("ao_viewport_guide: failed to register settings node.");
        return stat;
    }

    MHWRender::MRenderer* r = MHWRender::MRenderer::theRenderer();
    if (!r) return MS::kFailure;

    if (!gOverride)
    {
        gOverride = new AoViewportGuideOverride(kOverrideIdName);
        r->registerOverride(gOverride);
    }

    if (!ensureSettingsNodeExists())
    {
        MGlobal::displayWarning("ao_viewport_guide: settings node not found and could not be created.");
    }

    MGlobal::displayInfo("ao_viewport_guide: registered. Select 'AO Viewport Guide' from Renderer menu.");
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
    MStatus stat;
    MFnPlugin plugin(obj);

    MHWRender::MRenderer* r = MHWRender::MRenderer::theRenderer();
    if (r && gOverride)
    {
        r->deregisterOverride(gOverride);
        delete gOverride;
        gOverride = nullptr;
    }

    stat = plugin.deregisterNode(kSettingsNodeTypeId);
    if (stat != MS::kSuccess)
    {
        MGlobal::displayError("ao_viewport_guide: failed to deregister settings node.");
    }

    gSettingsNodeName.clear();
    MGlobal::displayInfo("ao_viewport_guide: unregistered.");
    return MS::kSuccess;
}
