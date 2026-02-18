// aoViewportGuideOverride.cpp (v0.3.1)
// NOTE: MHUDRenderOperation is NOT used (not available in this SDK)

#include "aoViewportGuideCommon.h"
#include "aoViewportGuideOverride.h"
#include "aoViewportGuideSettings.h"
#include "aoViewportGuideGate.h"

#include <maya/MFrameContext.h>
#include <maya/MUIDrawManager.h>
#include <maya/MPoint.h>

#include <cmath>

namespace AoViewportGuide
{
    class AoViewportGuideSceneRender : public MHWRender::MSceneRender
    {
    public:
        AoViewportGuideSceneRender(const MString& name)
            : MHWRender::MSceneRender(name) {}

        MHWRender::MClearOperation& clearOperation() override
        {
            const SettingsData s = AoViewportGuideSettings::read();
            if (s.bgEnable)
            {
                float c[4] = { s.bgColor.r, s.bgColor.g, s.bgColor.b, 1.0f };
                mClearOperation.setClearGradient(false);
                mClearOperation.setClearColor(c);
                mClearOperation.setMask(MHWRender::MClearOperation::kClearAll);
            }
            else
            {
                mClearOperation.setMask(MHWRender::MClearOperation::kClearAll);
            }
            return mClearOperation;
        }
    };

    class AoViewportGuideHUD : public MHWRender::MHUDRender
    {
    public:
        bool hasUIDrawables() const override { return true; }

        void addUIDrawables(MHWRender::MUIDrawManager& dm,
                            const MHWRender::MFrameContext& frameContext) override
        {
            const SettingsData s = AoViewportGuideSettings::read();
            if (!s.enable) return;

            int vpX=0, vpY=0, vpW=0, vpH=0;
            frameContext.getViewportDimensions(vpX, vpY, vpW, vpH);
            if (vpW < 10 || vpH < 10) return;

            const GateRect gate = computeGateRect(frameContext, vpX, vpY, vpW, vpH, s.followResolutionGate);

            dm.beginDrawable();

            if (s.gateBorderEnable && s.gateBorderOpacity > 0.0001f)
            {
                MColor bc = s.gateBorderColor;
                bc.a = clampf(s.gateBorderOpacity, 0.0f, 1.0f);

                dm.setColor(bc);
                dm.setLineWidth(s.gateBorderThickness);

                dm.line2d(MPoint(gate.left,  gate.bottom), MPoint(gate.right, gate.bottom));
                dm.line2d(MPoint(gate.right, gate.bottom), MPoint(gate.right, gate.top));
                dm.line2d(MPoint(gate.right, gate.top),    MPoint(gate.left,  gate.top));
                dm.line2d(MPoint(gate.left,  gate.top),    MPoint(gate.left,  gate.bottom));
            }

            MColor lc = s.lineColor;
            lc.a = clampf(s.lineOpacity, 0.0f, 1.0f);

            dm.setColor(lc);
            dm.setLineWidth(s.lineThickness);

            const double w = gate.right - gate.left;
            const double h = gate.top   - gate.bottom;

            if (s.guideType == 0)
            {
                const double x1 = gate.left + w / 3.0;
                const double x2 = gate.left + w * 2.0 / 3.0;
                const double y1 = gate.bottom + h / 3.0;
                const double y2 = gate.bottom + h * 2.0 / 3.0;

                dm.line2d(MPoint(x1, gate.bottom), MPoint(x1, gate.top));
                dm.line2d(MPoint(x2, gate.bottom), MPoint(x2, gate.top));
                dm.line2d(MPoint(gate.left, y1),   MPoint(gate.right, y1));
                dm.line2d(MPoint(gate.left, y2),   MPoint(gate.right, y2));
            }
            else if (s.guideType == 1)
            {
                const double cx = gate.left + w * 0.5;
                const double cy = gate.bottom + h * 0.5;

                dm.line2d(MPoint(cx, gate.bottom), MPoint(cx, gate.top));
                dm.line2d(MPoint(gate.left, cy),   MPoint(gate.right, cy));
            }
            else
            {
                const double cx = gate.left + w * 0.5;
                const double cy = gate.bottom + h * 0.5;
                const double r  = (w < h ? w : h) * 0.5;

                const int seg = 96;
                for (int i = 0; i < seg; ++i)
                {
                    const double a0 = (2.0 * 3.141592653589793) * (double)i / (double)seg;
                    const double a1 = (2.0 * 3.141592653589793) * (double)(i + 1) / (double)seg;

                    const double x0 = cx + std::cos(a0) * r;
                    const double y0 = cy + std::sin(a0) * r;
                    const double x1 = cx + std::cos(a1) * r;
                    const double y1 = cy + std::sin(a1) * r;

                    dm.line2d(MPoint(x0, y0), MPoint(x1, y1));
                }
            }

            dm.endDrawable();
        }
    };

    class AoViewportGuideRenderOverride : public MHWRender::MRenderOverride
    {
    public:
        AoViewportGuideRenderOverride(const MString& name)
            : MHWRender::MRenderOverride(name)
        {
            mScene   = new AoViewportGuideSceneRender("aoViewportGuide_scene");
            mHud     = new AoViewportGuideHUD();
            mPresent = new MHWRender::MPresentTarget("aoViewportGuide_present");
        }

        ~AoViewportGuideRenderOverride() override
        {
            delete mScene;
            delete mHud;
            delete mPresent;
        }

        MString uiName() const override { return kOverrideUiName; }

        MHWRender::DrawAPI supportedDrawAPIs() const override
        {
            return MHWRender::kAllDevices;
        }

        MStatus setup(const MString&) override
        {
            AoViewportGuideSettings::ensureNodeExists();
            return MS::kSuccess;
        }

        MStatus cleanup() override { return MS::kSuccess; }

        bool startOperationIterator() override
        {
            mIndex = 0;
            return true;
        }

        MHWRender::MRenderOperation* renderOperation() override
        {
            switch (mIndex)
            {
            case 0: return mScene;
            case 1: return mHud;     // IMPORTANT: use MHUDRender directly (no MHUDRenderOperation)
            case 2: return mPresent;
            default: return nullptr;
            }
        }

        bool nextRenderOperation() override
        {
            ++mIndex;
            return (mIndex < 3);
        }

    private:
        int mIndex = 0;
        AoViewportGuideSceneRender* mScene = nullptr;
        AoViewportGuideHUD*         mHud = nullptr;
        MHWRender::MPresentTarget*  mPresent = nullptr;
    };

    MHWRender::MRenderOverride* createOverride()
    {
        return new AoViewportGuideRenderOverride(kOverrideNameInternal);
    }

    void destroyOverride(MHWRender::MRenderOverride*& ptr)
    {
        delete ptr;
        ptr = nullptr;
    }
}
