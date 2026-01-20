# ao_viewport_guide

Maya 2025 Viewport 2.0 overlay guide plugin (C++ / .mll)

## Requirements
- Windows
- Autodesk Maya 2025
- Visual Studio 2022 (x64)
- CMake 3.20+

## Build (PowerShell)
> Set your Maya install path via `-DMAYA_LOCATION=...`

```powershell
cd E:\tool\ao_viewport_guide
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMAYA_LOCATION="C:\Program Files\Autodesk\Maya2025"
cmake --build build --config Release
