#pragma once
// AO Viewport Guide v0.2.3
// Settings node + read helper
//
// IMPORTANT:
// - Do NOT include <maya/MFnPlugin.h> here (avoid LNK2005 duplicates)

#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MColor.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>

struct AoViewportGuideSettingsData
{
    bool  enable = true;

    // Thirds lines
    float lineOpacity   = 1.0f;
    float lineThickness = 1.5f;
    MColor lineColor    = MColor(0.0f, 1.0f, 0.0f, 1.0f);

    // Resolution gate border
    bool  gateBorderEnable   = true;
    float gateBorderOpacity  = 1.0f;
    float gateBorderThickness= 3.0f;
    MColor gateBorderColor   = MColor(1.0f, 1.0f, 1.0f, 1.0f);

    // Outside matte
    bool  matteEnable  = false;
    float matteOpacity = 0.3f;
    MColor matteColor  = MColor(0.0f, 0.0f, 0.0f, 1.0f);

    // behavior
    bool followResolutionGate = true;

    // info
    bool foundNode = false;
};

namespace AoViewportGuideSettingsConst
{
    inline constexpr const char* kNodeTypeName = "aoViewportGuideSettings";
    inline constexpr const char* kDefaultNodeName = "aoViewportGuideSettings1";
}

// single definition in .cpp
extern const MTypeId  gAoViewportGuideSettingsTypeId;
extern const MString  gAoViewportGuideSettingsTypeName;

class AoViewportGuideSettingsNode : public MPxNode
{
public:
    static void* creator();
    static MStatus initialize();

    // attributes
    static MObject aEnable;

    static MObject aLineOpacity;
    static MObject aLineThickness;
    static MObject aLineColor;

    static MObject aGateBorderEnable;
    static MObject aGateBorderOpacity;
    static MObject aGateBorderThickness;
    static MObject aGateBorderColor;

    static MObject aMatteEnable;
    static MObject aMatteOpacity;
    static MObject aMatteColor;

    static MObject aFollowResolutionGate;
};

class AoViewportGuideSettings
{
public:
    // ensure a settings node exists in the scene, returns node name ("" if failed)
    static MString ensure();

    // read settings (defaults if node missing)
    static AoViewportGuideSettingsData read();

private:
    static bool findExisting(MString& outName);
    static bool createDefaultNode(MString& outName);
};
