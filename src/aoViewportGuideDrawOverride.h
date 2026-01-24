#pragma once

#include <maya/MPxDrawOverride.h>
#include <maya/MUserData.h>   // ★ MUserData はグローバル
#include <maya/MFrameContext.h>

class AOViewportGuideDrawOverride : public MHWRender::MPxDrawOverride
{
public:
    static MHWRender::MPxDrawOverride* creator(const MObject& obj);
    ~AOViewportGuideDrawOverride() override = default;

    MHWRender::DrawAPI supportedDrawAPIs() const override;
    bool isBounded(const MDagPath& objPath, const MDagPath& cameraPath) const override;
    MBoundingBox boundingBox(const MDagPath& objPath, const MDagPath& cameraPath) const override;

    // ★ Maya2025: MUserData はグローバル
    MUserData* prepareForDraw(
        const MDagPath& objPath,
        const MDagPath& cameraPath,
        const MHWRender::MFrameContext& frameContext,
        MUserData* oldData) override;

    bool hasUIDrawables() const override;

    // ★ Maya2025: 第4引数は const MUserData*（グローバル）
    // ※末尾 const は付けない（override一致のため）
    void addUIDrawables(
        const MDagPath& objPath,
        MHWRender::MUIDrawManager& drawManager,
        const MHWRender::MFrameContext& frameContext,
        const MUserData* data) override;

private:
    AOViewportGuideDrawOverride(const MObject& obj);

    static bool getRenderAspect(double& outAspect);
    static bool getViewportSize(const MHWRender::MFrameContext& ctx, int& outW, int& outH);
    static bool getCameraInfo(const MHWRender::MFrameContext& ctx, MDagPath& outCamPath);

    static void computeResolutionGateRect(
        int vpW, int vpH,
        double gateAspect,
        short filmFit,
        double overscan,
        double& outLeft, double& outBottom, double& outRight, double& outTop
    );
};
