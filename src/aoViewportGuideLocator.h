#pragma once
#include <maya/MPxLocatorNode.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>
#include <maya/MObject.h>

class AOViewportGuideLocator : public MPxLocatorNode
{
public:
    AOViewportGuideLocator() = default;
    ~AOViewportGuideLocator() override = default;

    static void* creator();
    static MStatus initialize();

    // Attributes (v0.2: enable only)
    static MObject aEnable;

    static const MString kTypeName;
    static const MString kDrawDbClassification;
    static const MString kDrawRegistrantId;
    static const MTypeId kTypeId;
};
