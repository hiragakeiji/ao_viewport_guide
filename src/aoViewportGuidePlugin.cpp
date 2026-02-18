// aoViewportGuidePlugin.cpp (v0.3.1)
// Plugin entry points only.

#include "aoViewportGuideCommon.h"
#include "aoViewportGuideOverride.h"
#include "aoViewportGuideSettings.h"

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MPxNode.h>
#include <maya/MViewport2Renderer.h>

namespace
{
    static MHWRender::MRenderOverride* gOverride = nullptr;
}

MStatus initializePlugin(MObject obj)
{
    MStatus stat;
    MFnPlugin plugin(obj, "ao", AoViewportGuide::kVersion, "Any", &stat);
    if (!stat) return stat;

    stat = plugin.registerNode(
        AoViewportGuide::kSettingsNodeTypeName,
        AoViewportGuide::AoViewportGuideSettingsNode::id,
        AoViewportGuide::SettingsNodeCreator,
        AoViewportGuide::SettingsNodeInitialize,
        MPxNode::kDependNode
    );
    if (!stat) return stat;

    AoViewportGuide::AoViewportGuideSettings::ensureNodeExists();

    MHWRender::MRenderer* r = MHWRender::MRenderer::theRenderer();
    if (r && !gOverride)
    {
        gOverride = AoViewportGuide::createOverride();
        r->registerOverride(gOverride);
    }

    MGlobal::displayInfo(MString("[ao_viewport_guide] Loaded ") + AoViewportGuide::kVersion);
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
    MStatus stat;
    MFnPlugin plugin(obj, "ao", AoViewportGuide::kVersion, "Any", &stat);
    if (!stat) return stat;

    MHWRender::MRenderer* r = MHWRender::MRenderer::theRenderer();
    if (r && gOverride)
    {
        r->deregisterOverride(gOverride);
        AoViewportGuide::destroyOverride(gOverride);
    }

    stat = plugin.deregisterNode(AoViewportGuide::AoViewportGuideSettingsNode::id);
    if (!stat) return stat;

    MGlobal::displayInfo("[ao_viewport_guide] Unloaded");
    return MS::kSuccess;
}
