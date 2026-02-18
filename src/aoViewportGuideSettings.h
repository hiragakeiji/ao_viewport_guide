#pragma once

#include <maya/MColor.h>
#include <maya/MStatus.h>
#include <maya/MTypeId.h>

namespace AoViewportGuide
{
    struct SettingsData
    {
        bool   enable = true;
        bool   followResolutionGate = true;

        // 0: Thirds, 1: Cross, 2: Circle
        int    guideType = 0;

        float  lineOpacity   = 1.0f;
        float  lineThickness = 2.0f;
        MColor lineColor     = MColor(0.0f, 1.0f, 0.0f, 1.0f);

        bool   gateBorderEnable    = true;
        float  gateBorderOpacity   = 1.0f;
        float  gateBorderThickness = 2.0f;
        MColor gateBorderColor     = MColor(1.0f, 1.0f, 1.0f, 1.0f);

        // background solid clear
        bool   bgEnable = false;
        MColor bgColor  = MColor(0.0f, 0.0f, 0.0f, 1.0f);
    };

    class AoViewportGuideSettingsNode
    {
    public:
        static MTypeId id;
    };

    class AoViewportGuideSettings
    {
    public:
        static bool ensureNodeExists();
        static SettingsData read();
    };

    // for plugin.registerNode
    void*   SettingsNodeCreator();
    MStatus SettingsNodeInitialize();
}
