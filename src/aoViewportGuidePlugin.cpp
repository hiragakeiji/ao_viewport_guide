// aoViewportGuidePlugin.cpp
// AO Viewport Guide v0.2.3
//
// IMPORTANT:
// - This is the ONLY .cpp that includes <maya/MFnPlugin.h>
//   to avoid LNK2005 duplicates (DllMain / MApiVersion / ADSK_PLUGIN_SIGNATURE)

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <maya/MViewport2Renderer.h> // for MHWRender::MRenderer etc

#include "aoViewportGuideOverride.h"
#include "aoViewportGuideSettings.h"

namespace
{
MHWRender::MRenderOverride* gOverride = nullptr;
}

MStatus initializePlugin(MObject obj)
{
    MStatus stat;
    MFnPlugin plugin(obj, "ao", "0.2.3", "Any", &stat);
    if (!stat) return stat;

    // register settings node
    stat = plugin.registerNode(
        AoViewportGuideSettingsConst::kNodeTypeName,
        gAoViewportGuideSettingsTypeId,
        AoViewportGuideSettingsNode::creator,
        AoViewportGuideSettingsNode::initialize,
        MPxNode::kDependNode
    );
    if (stat != MS::kSuccess)
    {
        MGlobal::displayError("[ao_viewport_guide] Failed to register settings node.");
        return stat;
    }

    // ensure node exists
    (void)AoViewportGuideSettings::ensure();

    // register render override (Renderer menu)
    MHWRender::MRenderer* r = MHWRender::MRenderer::theRenderer();
    if (!r)
    {
        MGlobal::displayError("[ao_viewport_guide] MRenderer::theRenderer() failed.");
        return MS::kFailure;
    }

    if (!gOverride)
    {
        gOverride = AoViewportGuide::createOverride();
        r->registerOverride(gOverride);
    }

    MGlobal::displayInfo("[ao_viewport_guide] Loaded v0.2.3 : AO Viewport Guide");
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
    MStatus stat;
    MFnPlugin plugin(obj);

    // deregister override
    MHWRender::MRenderer* r = MHWRender::MRenderer::theRenderer();
    if (r && gOverride)
    {
        r->deregisterOverride(gOverride);
        AoViewportGuide::destroyOverride(gOverride);
    }

    // deregister node
    stat = plugin.deregisterNode(gAoViewportGuideSettingsTypeId);
    if (stat != MS::kSuccess)
    {
        MGlobal::displayError("[ao_viewport_guide] Failed to deregister settings node.");
    }

    MGlobal::displayInfo("[ao_viewport_guide] Unloaded");
    return MS::kSuccess;
}
