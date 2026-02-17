// aoViewportGuideSettings.cpp
// AO Viewport Guide v0.2.3
//
// IMPORTANT:
// - Do NOT include <maya/MFnPlugin.h> here (avoid LNK2005 duplicates)

#include "aoViewportGuideSettings.h"

#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MItDependencyNodes.h>

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>

#include <algorithm>

const MTypeId gAoViewportGuideSettingsTypeId(0x0013A0F3); // ←衝突しない値に固定
const MString gAoViewportGuideSettingsTypeName(AoViewportGuideSettingsConst::kNodeTypeName);

// attrs
MObject AoViewportGuideSettingsNode::aEnable;

MObject AoViewportGuideSettingsNode::aLineOpacity;
MObject AoViewportGuideSettingsNode::aLineThickness;
MObject AoViewportGuideSettingsNode::aLineColor;

MObject AoViewportGuideSettingsNode::aGateBorderEnable;
MObject AoViewportGuideSettingsNode::aGateBorderOpacity;
MObject AoViewportGuideSettingsNode::aGateBorderThickness;
MObject AoViewportGuideSettingsNode::aGateBorderColor;

MObject AoViewportGuideSettingsNode::aMatteEnable;
MObject AoViewportGuideSettingsNode::aMatteOpacity;
MObject AoViewportGuideSettingsNode::aMatteColor;

MObject AoViewportGuideSettingsNode::aFollowResolutionGate;

void* AoViewportGuideSettingsNode::creator()
{
    return new AoViewportGuideSettingsNode();
}

MStatus AoViewportGuideSettingsNode::initialize()
{
    MStatus s;
    MFnNumericAttribute nAttr;

    aEnable = nAttr.create("enable", "en", MFnNumericData::kBoolean, true, &s);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aEnable);

    // Thirds
    aLineOpacity = nAttr.create("lineOpacity", "lop", MFnNumericData::kFloat, 1.0f, &s);
    nAttr.setMin(0.0f); nAttr.setMax(1.0f);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aLineOpacity);

    aLineThickness = nAttr.create("lineThickness", "lth", MFnNumericData::kFloat, 1.5f, &s);
    nAttr.setMin(0.5f); nAttr.setMax(20.0f);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aLineThickness);

    aLineColor = nAttr.createColor("lineColor", "lcl", &s);
    nAttr.setDefault(0.0f, 1.0f, 0.0f);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aLineColor);

    // Gate border
    aGateBorderEnable = nAttr.create("gateBorderEnable", "gbe", MFnNumericData::kBoolean, true, &s);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aGateBorderEnable);

    aGateBorderOpacity = nAttr.create("gateBorderOpacity", "gbo", MFnNumericData::kFloat, 1.0f, &s);
    nAttr.setMin(0.0f); nAttr.setMax(1.0f);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aGateBorderOpacity);

    aGateBorderThickness = nAttr.create("gateBorderThickness", "gbt", MFnNumericData::kFloat, 3.0f, &s);
    nAttr.setMin(0.5f); nAttr.setMax(50.0f);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aGateBorderThickness);

    aGateBorderColor = nAttr.createColor("gateBorderColor", "gbc", &s);
    nAttr.setDefault(1.0f, 1.0f, 1.0f);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aGateBorderColor);

    // Matte
    aMatteEnable = nAttr.create("matteEnable", "mte", MFnNumericData::kBoolean, false, &s);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aMatteEnable);

    aMatteOpacity = nAttr.create("matteOpacity", "mto", MFnNumericData::kFloat, 0.3f, &s);
    nAttr.setMin(0.0f); nAttr.setMax(1.0f);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aMatteOpacity);

    aMatteColor = nAttr.createColor("matteColor", "mtc", &s);
    nAttr.setDefault(0.0f, 0.0f, 0.0f);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aMatteColor);

    // Behavior
    aFollowResolutionGate = nAttr.create("followResolutionGate", "frg", MFnNumericData::kBoolean, true, &s);
    nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
    addAttribute(aFollowResolutionGate);

    return MS::kSuccess;
}

bool AoViewportGuideSettings::findExisting(MString& outName)
{
    MItDependencyNodes it(MFn::kPluginDependNode);
    for (; !it.isDone(); it.next())
    {
        MObject obj = it.item();
        MFnDependencyNode fn(obj);
        if (fn.typeId() == gAoViewportGuideSettingsTypeId)
        {
            outName = fn.name();
            return true;
        }
    }
    return false;
}

bool AoViewportGuideSettings::createDefaultNode(MString& outName)
{
    // user-requested: create via MEL
    MString cmd;
    cmd += "if (!`objExists \"";
    cmd += AoViewportGuideSettingsConst::kDefaultNodeName;
    cmd += "\"`) { createNode ";
    cmd += AoViewportGuideSettingsConst::kNodeTypeName;
    cmd += " -n \"";
    cmd += AoViewportGuideSettingsConst::kDefaultNodeName;
    cmd += "\"; }";

    if (MGlobal::executeCommand(cmd, false, false) != MS::kSuccess)
        return false;

    // resolve created node (type-based)
    return findExisting(outName);
}

MString AoViewportGuideSettings::ensure()
{
    MString name;
    if (findExisting(name))
        return name;

    if (createDefaultNode(name))
        return name;

    return "";
}

static inline float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

AoViewportGuideSettingsData AoViewportGuideSettings::read()
{
    AoViewportGuideSettingsData s;

    MString nodeName = ensure();
    if (nodeName.length() == 0)
        return s;

    MSelectionList sl;
    if (sl.add(nodeName) != MS::kSuccess)
        return s;

    MObject obj;
    if (sl.getDependNode(0, obj) != MS::kSuccess || obj.isNull())
        return s;

    MFnDependencyNode fn(obj);
    if (fn.typeId() != gAoViewportGuideSettingsTypeId)
        return s;

    s.foundNode = true;

    auto readBool = [&](const MObject& attr, bool& outVal)
    {
        MPlug p = fn.findPlug(attr, true);
        if (!p.isNull()) outVal = p.asBool();
    };
    auto readFloat01 = [&](const MObject& attr, float& outVal)
    {
        MPlug p = fn.findPlug(attr, true);
        if (!p.isNull()) outVal = clampf(p.asFloat(), 0.0f, 1.0f);
    };
    auto readFloat = [&](const MObject& attr, float& outVal, float lo, float hi)
    {
        MPlug p = fn.findPlug(attr, true);
        if (!p.isNull()) outVal = clampf(p.asFloat(), lo, hi);
    };
    auto readColor = [&](const MObject& attr, MColor& outCol)
    {
        MPlug p = fn.findPlug(attr, true);
        if (!p.isNull() && p.numChildren() >= 3)
        {
            outCol = MColor(
                clampf(p.child(0).asFloat(), 0.0f, 1.0f),
                clampf(p.child(1).asFloat(), 0.0f, 1.0f),
                clampf(p.child(2).asFloat(), 0.0f, 1.0f),
                1.0f
            );
        }
    };

    readBool(AoViewportGuideSettingsNode::aEnable, s.enable);

    readFloat01(AoViewportGuideSettingsNode::aLineOpacity, s.lineOpacity);
    readFloat(AoViewportGuideSettingsNode::aLineThickness, s.lineThickness, 0.5f, 50.0f);
    readColor(AoViewportGuideSettingsNode::aLineColor, s.lineColor);

    readBool(AoViewportGuideSettingsNode::aGateBorderEnable, s.gateBorderEnable);
    readFloat01(AoViewportGuideSettingsNode::aGateBorderOpacity, s.gateBorderOpacity);
    readFloat(AoViewportGuideSettingsNode::aGateBorderThickness, s.gateBorderThickness, 0.5f, 100.0f);
    readColor(AoViewportGuideSettingsNode::aGateBorderColor, s.gateBorderColor);

    readBool(AoViewportGuideSettingsNode::aMatteEnable, s.matteEnable);
    readFloat01(AoViewportGuideSettingsNode::aMatteOpacity, s.matteOpacity);
    readColor(AoViewportGuideSettingsNode::aMatteColor, s.matteColor);

    readBool(AoViewportGuideSettingsNode::aFollowResolutionGate, s.followResolutionGate);

    return s;
}
