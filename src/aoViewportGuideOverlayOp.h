#pragma once
#include <maya/MUserRenderOperation.h>
#include <maya/MString.h>

class AOViewportGuideOverlayOp : public MHWRender::MUserRenderOperation
{
public:
    explicit AOViewportGuideOverlayOp(const MString& name);
    ~AOViewportGuideOverlayOp() override = default;

    MStatus execute(const MHWRender::MDrawContext& context) override;

private:
    static bool getRenderAspect(double& outAspect);
    static bool getViewportSize(const MHWRender::MFrameContext& fc, int& outW, int& outH);
    static bool getCurrentCameraPath(const MHWRender::MFrameContext& fc, MDagPath& outCamPath);

    static void computeGateRect(
        int vpW, int vpH,
        double gateAspect,
        short filmFit,
        double overscan,
        double& l, double& b, double& r, double& t
    );
};
