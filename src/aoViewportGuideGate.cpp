// aoViewportGuideGate.cpp (v0.3.1)

#include "aoViewportGuideGate.h"

#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>

namespace AoViewportGuide
{
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

        outAspect = (double)w / (double)h;
        return true;
    }

    struct CameraGateParams
    {
        bool   ok = false;
        double overscan = 1.0;
        int    filmFit = 3; // 0 Fill, 1 Horizontal, 2 Vertical, 3 Overscan
    };

    static CameraGateParams getCameraGateParams(const MHWRender::MFrameContext& frameContext)
    {
        CameraGateParams p;

        MStatus stat;
        MDagPath camPath = frameContext.getCurrentCameraPath(&stat);
        if (!stat) return p;

        MFnCamera fnCam(camPath, &stat);
        if (!stat) return p;

        p.ok = true;
        p.overscan = fnCam.overscan();
        p.filmFit  = (int)fnCam.filmFit();
        return p;
    }

    GateRect computeGateRect(const MHWRender::MFrameContext& frameContext,
                             int vpX, int vpY, int vpW, int vpH,
                             bool followResolutionGate)
    {
        GateRect g;
        if (vpW <= 0 || vpH <= 0) return g;

        const double viewW  = (double)vpW;
        const double viewH  = (double)vpH;
        const double viewAR = viewW / viewH;

        double gateAR = viewAR;
        if (followResolutionGate)
        {
            double ar = 1.0;
            if (getDefaultResolutionAspect(ar))
                gateAR = ar;
        }

        const CameraGateParams camP = getCameraGateParams(frameContext);
        const double overscan = (camP.ok && camP.overscan > 0.0001) ? camP.overscan : 1.0;
        const int filmFit = camP.ok ? camP.filmFit : 3;

        double rectW = viewW;
        double rectH = viewH;

        switch (filmFit)
        {
        case 1: // Horizontal
            rectW = viewW;
            rectH = rectW / gateAR;
            break;
        case 2: // Vertical
            rectH = viewH;
            rectW = rectH * gateAR;
            break;
        case 0: // Fill
            if (viewAR >= gateAR) { rectW = viewW; rectH = rectW / gateAR; }
            else                  { rectH = viewH; rectW = rectH * gateAR; }
            break;
        case 3: // Overscan
        default:
            if (viewAR >= gateAR) { rectH = viewH; rectW = rectH * gateAR; }
            else                  { rectW = viewW; rectH = rectW / gateAR; }
            break;
        }

        rectW /= overscan;
        rectH /= overscan;

        const double rectX = (viewW - rectW) * 0.5;
        const double rectY = (viewH - rectH) * 0.5;

        g.left   = (double)vpX + rectX;
        g.bottom = (double)vpY + rectY;
        g.right  = g.left + rectW;
        g.top    = g.bottom + rectH;
        return g;
    }
}
