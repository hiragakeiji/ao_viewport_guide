#include "aoViewportGuideLocator.h"

#include <maya/MFnNumericAttribute.h>

const MString AOViewportGuideLocator::kTypeName("aoViewportGuideLocator");
const MString AOViewportGuideLocator::kDrawDbClassification("drawdb/geometry/aoViewportGuideLocator");
const MString AOViewportGuideLocator::kDrawRegistrantId("ao_viewport_guide");

// NOTE: TypeId は衝突しないようにプロジェクトで管理してください
const MTypeId  AOViewportGuideLocator::kTypeId(0x0012F8A2);

MObject AOViewportGuideLocator::aEnable;

void* AOViewportGuideLocator::creator()
{
    return new AOViewportGuideLocator();
}

MStatus AOViewportGuideLocator::initialize()
{
    MFnNumericAttribute nAttr;

    aEnable = nAttr.create("enable", "en", MFnNumericData::kBoolean, true);
    nAttr.setKeyable(true);
    nAttr.setStorable(true);

    addAttribute(aEnable);

    return MS::kSuccess;
}
