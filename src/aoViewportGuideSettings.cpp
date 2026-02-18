// aoViewportGuideSettings.cpp (v0.3.1)

#include "aoViewportGuideCommon.h"
#include "aoViewportGuideSettings.h"

#include <maya/MPxNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MSelectionList.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MGlobal.h>

namespace AoViewportGuide
{
    MTypeId AoViewportGuideSettingsNode::id(0x0013A0F2);

    class AoViewportGuideSettingsNodeImpl : public MPxNode
    {
    public:
        static void* creator() { return new AoViewportGuideSettingsNodeImpl(); }
        static MStatus initialize();

        static MObject aEnable;
        static MObject aFollowResolutionGate;
        static MObject aGuideType;

        static MObject aLineOpacity;
        static MObject aLineThickness;
        static MObject aLineColor;

        static MObject aGateBorderEnable;
        static MObject aGateBorderOpacity;
        static MObject aGateBorderThickness;
        static MObject aGateBorderColor;

        static MObject aBgEnable;
        static MObject aBgColor;
    };

    MObject AoViewportGuideSettingsNodeImpl::aEnable;
    MObject AoViewportGuideSettingsNodeImpl::aFollowResolutionGate;
    MObject AoViewportGuideSettingsNodeImpl::aGuideType;

    MObject AoViewportGuideSettingsNodeImpl::aLineOpacity;
    MObject AoViewportGuideSettingsNodeImpl::aLineThickness;
    MObject AoViewportGuideSettingsNodeImpl::aLineColor;

    MObject AoViewportGuideSettingsNodeImpl::aGateBorderEnable;
    MObject AoViewportGuideSettingsNodeImpl::aGateBorderOpacity;
    MObject AoViewportGuideSettingsNodeImpl::aGateBorderThickness;
    MObject AoViewportGuideSettingsNodeImpl::aGateBorderColor;

    MObject AoViewportGuideSettingsNodeImpl::aBgEnable;
    MObject AoViewportGuideSettingsNodeImpl::aBgColor;

    MStatus AoViewportGuideSettingsNodeImpl::initialize()
    {
        MStatus s;
        MFnNumericAttribute nAttr;
        MFnEnumAttribute    eAttr;

        aEnable = nAttr.create("enable", "en", MFnNumericData::kBoolean, true, &s);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aEnable);

        aFollowResolutionGate = nAttr.create("followResolutionGate", "frg", MFnNumericData::kBoolean, true, &s);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aFollowResolutionGate);

        aGuideType = eAttr.create("guideType", "gt", 0, &s);
        eAttr.addField("Thirds (3x3)", 0);
        eAttr.addField("Cross", 1);
        eAttr.addField("Circle", 2);
        eAttr.setKeyable(true); eAttr.setStorable(true); eAttr.setChannelBox(true);
        addAttribute(aGuideType);

        aLineOpacity = nAttr.create("lineOpacity", "lop", MFnNumericData::kFloat, 1.0f, &s);
        nAttr.setMin(0.0f); nAttr.setMax(1.0f);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aLineOpacity);

        aLineThickness = nAttr.create("lineThickness", "lth", MFnNumericData::kFloat, 2.0f, &s);
        nAttr.setMin(0.5f); nAttr.setMax(50.0f);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aLineThickness);

        aLineColor = nAttr.createColor("lineColor", "lcol", &s);
        nAttr.setDefault(0.0f, 1.0f, 0.0f);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aLineColor);

        aGateBorderEnable = nAttr.create("gateBorderEnable", "gbe", MFnNumericData::kBoolean, true, &s);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aGateBorderEnable);

        aGateBorderOpacity = nAttr.create("gateBorderOpacity", "gbo", MFnNumericData::kFloat, 1.0f, &s);
        nAttr.setMin(0.0f); nAttr.setMax(1.0f);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aGateBorderOpacity);

        aGateBorderThickness = nAttr.create("gateBorderThickness", "gbt", MFnNumericData::kFloat, 2.0f, &s);
        nAttr.setMin(0.5f); nAttr.setMax(50.0f);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aGateBorderThickness);

        aGateBorderColor = nAttr.createColor("gateBorderColor", "gbc", &s);
        nAttr.setDefault(1.0f, 1.0f, 1.0f);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aGateBorderColor);

        aBgEnable = nAttr.create("bgEnable", "bge", MFnNumericData::kBoolean, false, &s);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aBgEnable);

        aBgColor = nAttr.createColor("bgColor", "bgc", &s);
        nAttr.setDefault(0.0f, 0.0f, 0.0f);
        nAttr.setKeyable(true); nAttr.setStorable(true); nAttr.setChannelBox(true);
        addAttribute(aBgColor);

        return MS::kSuccess;
    }

    static bool getNodeByName(const char* name, MObject& outObj)
    {
        MSelectionList sl;
        if (sl.add(name) != MS::kSuccess) return false;
        if (sl.getDependNode(0, outObj) != MS::kSuccess) return false;
        return !outObj.isNull();
    }

    bool AoViewportGuideSettings::ensureNodeExists()
    {
        MObject obj;
        if (getNodeByName(kSettingsNodeName, obj))
            return true;

        MString cmd;
        cmd += "if (!`objExists \"";
        cmd += kSettingsNodeName;
        cmd += "\"`) {";
        cmd += "string $n = `createNode ";
        cmd += kSettingsNodeTypeName;
        cmd += "`; ";
        cmd += "rename $n \"";
        cmd += kSettingsNodeName;
        cmd += "\"; }";

        return (MGlobal::executeCommand(cmd, false, false) == MS::kSuccess);
    }

    SettingsData AoViewportGuideSettings::read()
    {
        SettingsData s;

        MObject obj;
        if (!getNodeByName(kSettingsNodeName, obj))
            return s;

        MFnDependencyNode fn(obj);

        auto getBool = [&](const char* attr, bool& out)
        {
            MPlug p = fn.findPlug(attr, true);
            if (!p.isNull()) out = p.asBool();
        };
        auto getInt = [&](const char* attr, int& out)
        {
            MPlug p = fn.findPlug(attr, true);
            if (!p.isNull()) out = p.asInt();
        };
        auto getFloat = [&](const char* attr, float& out)
        {
            MPlug p = fn.findPlug(attr, true);
            if (!p.isNull()) out = p.asFloat();
        };
        auto getColor = [&](const char* attr, MColor& out)
        {
            MPlug p = fn.findPlug(attr, true);
            if (!p.isNull() && p.numChildren() >= 3)
            {
                out = MColor(
                    p.child(0).asFloat(),
                    p.child(1).asFloat(),
                    p.child(2).asFloat(),
                    1.0f
                );
            }
        };

        getBool("enable", s.enable);
        getBool("followResolutionGate", s.followResolutionGate);
        getInt ("guideType", s.guideType);

        getFloat("lineOpacity", s.lineOpacity);
        getFloat("lineThickness", s.lineThickness);
        getColor("lineColor", s.lineColor);

        getBool ("gateBorderEnable", s.gateBorderEnable);
        getFloat("gateBorderOpacity", s.gateBorderOpacity);
        getFloat("gateBorderThickness", s.gateBorderThickness);
        getColor("gateBorderColor", s.gateBorderColor);

        getBool ("bgEnable", s.bgEnable);
        getColor("bgColor", s.bgColor);

        s.lineOpacity       = clampf(s.lineOpacity, 0.0f, 1.0f);
        s.gateBorderOpacity = clampf(s.gateBorderOpacity, 0.0f, 1.0f);

        s.lineThickness       = clampf(s.lineThickness, 0.5f, 50.0f);
        s.gateBorderThickness = clampf(s.gateBorderThickness, 0.5f, 50.0f);

        if (s.guideType < 0) s.guideType = 0;
        if (s.guideType > 2) s.guideType = 2;

        return s;
    }

    void* SettingsNodeCreator()
    {
        return AoViewportGuideSettingsNodeImpl::creator();
    }

    MStatus SettingsNodeInitialize()
    {
        return AoViewportGuideSettingsNodeImpl::initialize();
    }
}
