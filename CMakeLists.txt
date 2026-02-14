cmake_minimum_required(VERSION 3.20)
project(ao_viewport_guide LANGUAGES CXX)

set(MAYA_LOCATION "" CACHE PATH "Maya install path (e.g. C:/Program Files/Autodesk/Maya2025)")
set(DEVKIT_BASE  "" CACHE PATH "Optional: DevKit base path (e.g. E:/mayadev/devkitBase)")

if(NOT MAYA_LOCATION)
  message(FATAL_ERROR "MAYA_LOCATION is not set. ex) -DMAYA_LOCATION=\"C:/Program Files/Autodesk/Maya2025\"")
endif()

add_library(ao_viewport_guide SHARED
  src/aoViewportGuidePlugin.cpp
)

target_compile_features(ao_viewport_guide PRIVATE cxx_std_17)

if(WIN32)
  set_target_properties(ao_viewport_guide PROPERTIES SUFFIX ".mll")
  target_compile_definitions(ao_viewport_guide PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32
    _WINDOWS
    NT_PLUGIN
  )
endif()

# include dirs (Maya + optional DevKit)
target_include_directories(ao_viewport_guide PRIVATE
  "${MAYA_LOCATION}/include"
)

if(DEVKIT_BASE)
  target_include_directories(ao_viewport_guide PRIVATE
    "${DEVKIT_BASE}/include"
    "${DEVKIT_BASE}/devkit/include"
  )
endif()

target_link_directories(ao_viewport_guide PRIVATE
  "${MAYA_LOCATION}/lib"
)

target_link_libraries(ao_viewport_guide PRIVATE
  OpenMaya
  OpenMayaRender
  Foundation
)

# もし unresolved externals が出たら追加：
# target_link_libraries(ao_viewport_guide PRIVATE OpenMayaUI)
