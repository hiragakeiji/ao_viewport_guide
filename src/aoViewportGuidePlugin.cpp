// ao_frameguide.cpp (Maya 2025 / Windows)
// Viewport 2.0 Render Override: Scene -> HUD -> Present
// + Settings node: aoFrameGuideSettings (auto-create on plugin load)
//   - enable (bool)
//   - opacity (float 0..1)
//   - color (float3 RGB 0..1)
//   - thickness (float pixels)
//   - guideType (enum) 0:Thirds, 1:GoldenSpiral, 2:VanishingPoint, 3:CenterCross, 4:Custom (reserved)
// NOTE: Currently only Thirds(0) is implemented in draw.

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
constexpr const char* kOverrideIdName = "ao_frameguide";
constexpr const char* kOverrideUiName = "AO Grid Guide";

constexpr const char* kSettingsNodeTypeName = "aoFrameGuideSettings";
// 適当なID（衝突しない範囲で固定してください）
static const MTypeId kSettingsNodeTypeId(0x0013A0F1);

// ロード時に作るノード名（既にあればそれを使う）
constexpr const char* kAutoNodeName = "aoFrameGuideSettings1";
static MString gSettingsNodeName; // 見つけた/作ったノード名を保持（あれば高速）

// ------------------------------------------------------------
// Defaults (used when no settings node exists)
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
class AoFrameGuideSettingsNode : public MPxNode
{
public:
	static void* creator() { return new AoFrameGuideSettingsNode(); }
	static MStatus initialize();

	static MObject aEnable;
	static MObject aOpacity;
	static MObject aColor;
	static MObject aThickness;
	static MObject aGuideType; // enum
};

MObject AoFrameGuideSettingsNode::aEnable;
MObject AoFrameGuideSettingsNode::aOpacity;
MObject AoFrameGuideSettingsNode::aColor;
MObject AoFrameGuideSettingsNode::aThickness;
MObject AoFrameGuideSettingsNode::aGuideType;

MStatus AoFrameGuideSettingsNode::initialize()
{
	MStatus s;

	// bool/float/color
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

	// enum: guideType
	MFnEnumAttribute eAttr;
	aGuideType = eAttr.create("guideType", "gt", kDefaultGuideType, &s);
	eAttr.addField("Thirds (3x3)", 0);
	eAttr.addField("Golden Spiral", 1);
	eAttr.addField("Vanishing Point", 2);
	eAttr.addField("Center Cross", 3);
	eAttr.addField("Custom", 4);
	eAttr.setKeyable(true);
	eAttr.setStorable(true);
	eAttr.setChannelBox(true);
	addAttribute(aGuideType);

	return MS::kSuccess;
}

// ------------------------------------------------------------
// Find/Create settings node
//  - If exists: remember its name
//  - If not: create it with MGlobal::executeCommand(createNode ...)
// ------------------------------------------------------------
static bool ensureSettingsNodeExists()
{
	// すでに名前が確定してるなら存在確認だけ
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

	// 探す
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

	// 無いので作る（ユーザー希望：executeCommand）
	{
		MString cmd;
		// -n は同名があると失敗するので、失敗しても createNode 単体で通るようにする
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

	// 作れたはずなので再探索
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
// Read settings from scene
// ------------------------------------------------------------
struct GuideSettings
{
	bool  enable   = kDefaultEnable;
	float opacity  = kDefaultOpacity;
	float thicknessPx = kDefaultThickPx;
	MColor colorRGB = kDefaultColorRGB;
	int   guideType = kDefaultGuideType;
	bool  foundNode = false;
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

	gs.foundNode = true;

	// enable
	{
		MPlug p = fn.findPlug(AoFrameGuideSettingsNode::aEnable, true);
		if (!p.isNull()) gs.enable = p.asBool();
	}
	// opacity
	{
		MPlug p = fn.findPlug(AoFrameGuideSettingsNode::aOpacity, true);
		if (!p.isNull()) gs.opacity = clampf(p.asFloat(), 0.0f, 1.0f);
	}
	// thickness
	{
		MPlug p = fn.findPlug(AoFrameGuideSettingsNode::aThickness, true);
		if (!p.isNull()) gs.thicknessPx = clampf(p.asFloat(), 0.5f, 10.0f);
	}
	// color
	{
		MPlug p = fn.findPlug(AoFrameGuideSettingsNode::aColor, true);
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
	// guideType
	{
		MPlug p = fn.findPlug(AoFrameGuideSettingsNode::aGuideType, true);
		if (!p.isNull()) gs.guideType = p.asInt();
	}

	return gs;
}

// ------------------------------------------------------------
// Maya "Colors" 設定からRGB取得
// ------------------------------------------------------------
static bool getDisplayRGBColor(const char* name, MColor& out)
{
	MDoubleArray v;
	MString cmd = "displayRGBColor -q ";
	cmd += name;

	if (MGlobal::executeCommand(cmd, v, false, false) == MS::kSuccess && v.length() >= 3)
	{
		out = MColor(
			static_cast<float>(v[0]),
			static_cast<float>(v[1]),
			static_cast<float>(v[2]),
			1.0f
		);
		return true;
	}
	return false;
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
// Maya2025: getCurrentCameraPath(MStatus*) returns MDagPath
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

	// fitResolutionGate
	MFnDependencyNode camDep(camPath.node(), &cs);
	if (cs == MS::kSuccess)
	{
		MStatus ps;
		MPlug fitPlug = camDep.findPlug("fitResolutionGate", true, &ps);
		if (ps == MS::kSuccess && !fitPlug.isNull())
		{
			// 想定: 0=Fill, 1=Overscan（逆ならこの1行を反転）
			const int v = fitPlug.asInt();
			p.hasFitResolutionGate = true;
			p.fitIsFill = (v == 0);
			// 逆なら：p.fitIsFill = (v == 1);
		}
	}

	p.ok = true;
	return p;
}

// ------------------------------------------------------------
// SceneRender: background follows Maya Colors
// ------------------------------------------------------------
class AoSceneRender : public MHWRender::MSceneRender
{
public:
	AoSceneRender(const MString& name) : MHWRender::MSceneRender(name) {}

	MHWRender::MClearOperation& clearOperation() override
	{
		MColor bg, top, bottom;
		const bool hasTop    = getDisplayRGBColor("backgroundTop", top);
		const bool hasBottom = getDisplayRGBColor("backgroundBottom", bottom);
		const bool hasBg     = getDisplayRGBColor("background", bg);

		float c1[4] = { 0.f, 0.f, 0.f, 1.f };
		float c2[4] = { 0.f, 0.f, 0.f, 1.f };

		if (hasTop && hasBottom)
		{
			c1[0] = top.r;    c1[1] = top.g;    c1[2] = top.b;
			c2[0] = bottom.r; c2[1] = bottom.g; c2[2] = bottom.b;

			mClearOperation.setClearGradient(true);
			mClearOperation.setClearColor(c1);
			mClearOperation.setClearColor2(c2);
		}
		else if (hasBg)
		{
			c1[0] = bg.r; c1[1] = bg.g; c1[2] = bg.b;
			mClearOperation.setClearGradient(false);
			mClearOperation.setClearColor(c1);
		}
		else
		{
			mClearOperation.setClearGradient(false);
			mClearOperation.setClearColor(c1);
		}

		mClearOperation.setMask(MHWRender::MClearOperation::kClearAll);
		return mClearOperation;
	}
};

// ------------------------------------------------------------
// HUDRender: draw guide (currently Thirds only)
// viewport-local 0..W, 0..H
// ------------------------------------------------------------
class AoGuideHUD : public MHWRender::MHUDRender
{
public:
	bool hasUIDrawables() const override { return true; }

	void addUIDrawables(MHWRender::MUIDrawManager& dm,
	                    const MHWRender::MFrameContext& frameContext) override
	{
		const GuideSettings gs = readGuideSettings();
		if (!gs.enable)
			return;

		// guideType dispatch (MVP: Thirds only)
		if (gs.guideType != 0)
		{
			// まだ未実装のガイドは描かない（将来ここに分岐を追加）
			return;
		}

		int vpX=0, vpY=0, vpW=0, vpH=0;
		frameContext.getViewportDimensions(vpX, vpY, vpW, vpH);
		if (vpW < 10 || vpH < 10)
			return;

		const double viewW = static_cast<double>(vpW);
		const double viewH = static_cast<double>(vpH);

		// aspect (MVP)
		double renderAspect = viewW / viewH;
		if (kFollowResolutionGate)
		{
			double ar = 1.0;
			if (getDefaultResolutionAspect(ar))
				renderAspect = ar;
		}

		// camera params
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

		// overscan shrink
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

		// Thirds lines
		dm.line2d(MPoint(x1, bottom), MPoint(x1, top));
		dm.line2d(MPoint(x2, bottom), MPoint(x2, top));
		dm.line2d(MPoint(left, y1),   MPoint(right, y1));
		dm.line2d(MPoint(left, y2),   MPoint(right, y2));

		dm.endDrawable();

		if (kShowDebugText)
		{
			char buf[256];
			sprintf_s(buf, "node=%s type=%d op=%.2f th=%.2f",
			          (gSettingsNodeName.length() > 0 ? gSettingsNodeName.asChar() : "none"),
			          gs.guideType, gs.opacity, gs.thicknessPx);

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
class AoFrameGuideOverride : public MHWRender::MRenderOverride
{
public:
	AoFrameGuideOverride(const MString& name)
		: MHWRender::MRenderOverride(name)
	{
		mScene   = new AoSceneRender("ao_frameguide_scene");
		mHud     = new AoGuideHUD();
		mPresent = new MHWRender::MPresentTarget("ao_frameguide_present");
	}

	~AoFrameGuideOverride() override
	{
		delete mScene;
		delete mHud;
		delete mPresent;
	}

	MString uiName() const override { return kOverrideUiName; }

	MHWRender::DrawAPI supportedDrawAPIs() const override
	{
		return MHWRender::kAllDevices; // OpenGL/DX11
	}

	MStatus setup(const MString&) override { return MS::kSuccess; }
	MStatus cleanup() override { return MS::kSuccess; }

	bool startOperationIterator() override { mIndex = 0; return true; }

	MHWRender::MRenderOperation* renderOperation() override
	{
		switch (mIndex)
		{
		case 0: return mScene;
		case 1: return mHud;
		case 2: return mPresent;
		default: return nullptr;
		}
	}

	bool nextRenderOperation() override
	{
		++mIndex;
		return (mIndex < 3);
	}

private:
	int mIndex = 0;
	AoSceneRender* mScene = nullptr;
	AoGuideHUD* mHud = nullptr;
	MHWRender::MPresentTarget* mPresent = nullptr;
};

static AoFrameGuideOverride* gOverride = nullptr;

} // namespace

// ------------------------------------------------------------
// Plugin entry points
// ------------------------------------------------------------
MStatus initializePlugin(MObject obj)
{
	MFnPlugin plugin(obj, "ao", "0.4", "Any");

	// Settings Node
	{
		MStatus s = plugin.registerNode(
			kSettingsNodeTypeName,
			kSettingsNodeTypeId,
			AoFrameGuideSettingsNode::creator,
			AoFrameGuideSettingsNode::initialize,
			MPxNode::kDependNode
		);
		if (s != MS::kSuccess)
		{
			MGlobal::displayError("ao_frameguide: failed to register settings node.");
			return s;
		}
	}

	// Render Override
	MHWRender::MRenderer* r = MHWRender::MRenderer::theRenderer();
	if (!r) return MS::kFailure;

	if (!gOverride)
	{
		gOverride = new AoFrameGuideOverride(kOverrideIdName);
		r->registerOverride(gOverride);
	}

	// Auto-create settings node (user requested)
	if (!ensureSettingsNodeExists())
	{
		MGlobal::displayWarning("ao_frameguide: settings node not found and could not be created.");
	}
	else
	{
		MString msg = "ao_frameguide: settings node = ";
		msg += gSettingsNodeName;
		MGlobal::displayInfo(msg);
	}

	MGlobal::displayInfo("ao_frameguide: registered. Select 'AO Grid Guide' from Renderer menu.");
	return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
	MFnPlugin plugin(obj);

	// Render Override
	MHWRender::MRenderer* r = MHWRender::MRenderer::theRenderer();
	if (r && gOverride)
	{
		r->deregisterOverride(gOverride);
		delete gOverride;
		gOverride = nullptr;
	}

	// Settings Node
	{
		MStatus s = plugin.deregisterNode(kSettingsNodeTypeId);
		if (s != MS::kSuccess)
		{
			MGlobal::displayError("ao_frameguide: failed to deregister settings node.");
		}
	}

	gSettingsNodeName.clear();

	MGlobal::displayInfo("ao_frameguide: unregistered.");
	return MS::kSuccess;
}
