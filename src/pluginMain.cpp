#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MPxCommand.h>
#include <maya/MAnimControl.h>
#include <maya/MTime.h>
#include <maya/MString.h>

#include <cstdio> // snprintf

static const char* kHudName = "aoViewportGuideHUD";
static const char* kCmdName = "aoViewportGuideValue";

// HUDが毎回呼ぶ「表示文字を返すだけ」のコマンド
class AoViewportGuideValueCmd : public MPxCommand
{
public:
    static void* creator() { return new AoViewportGuideValueCmd(); }
    bool isUndoable() const override { return false; }

    MStatus doIt(const MArgList&) override
    {
        const MTime t = MAnimControl::currentTime();
        const int frame = (int)t.as(MTime::uiUnit());

        char buf[128];
        std::snprintf(buf, sizeof(buf), "ao_viewport_guide v0.1.0 | frame: %d", frame);

        setResult(MString(buf));
        return MS::kSuccess;
    }
};

// HUDを作る（Viewportに表示）
static void createHud()
{
    // 既にあるなら消す（再ロード時に重複しない）
    MGlobal::executeCommand(
        "if (`headsUpDisplay -exists " + MString(kHudName) + "`) headsUpDisplay -remove " + MString(kHudName) + ";",
        false, false
    );

    // HUD追加（位置は section/block で調整）
    MString cmd;
    cmd += "headsUpDisplay -section 2 -block 0 -label \"GUIDE\" -command \"";
    cmd += kCmdName;
    cmd += "\" ";
    cmd += kHudName;
    cmd += ";";

    MGlobal::executeCommand(cmd, false, false);
}

static void removeHud()
{
    MGlobal::executeCommand(
        "if (`headsUpDisplay -exists " + MString(kHudName) + "`) headsUpDisplay -remove " + MString(kHudName) + ";",
        false, false
    );
}

MStatus initializePlugin(MObject obj)
{
    MFnPlugin plugin(obj, "ao", "0.1.0", "Any");

    // HUD用コマンド登録
    MStatus stat = plugin.registerCommand(kCmdName, AoViewportGuideValueCmd::creator);
    if (!stat) return stat;

    // HUD生成
    createHud();

    MGlobal::displayInfo("[ao_viewport_guide] loaded (HUD shown).");
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
    MFnPlugin plugin(obj);

    // HUD削除
    removeHud();

    // コマンド解除
    MStatus stat = plugin.deregisterCommand(kCmdName);
    if (!stat) return stat;

    MGlobal::displayInfo("[ao_viewport_guide] unloaded (HUD removed).");
    return MS::kSuccess;
}
