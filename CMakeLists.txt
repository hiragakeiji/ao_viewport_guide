cmake_minimum_required(VERSION 3.20)

project(ao_frameguide VERSION 0.0.1 LANGUAGES CXX)

# ---- Paths (edit or pass by -D) -------------------------------------------
set(MAYA_LOCATION "" CACHE PATH "Maya installation root (e.g. C:/Program Files/Autodesk/Maya2025)")
set(DEVKIT_BASE   "" CACHE PATH "DevKit base root (e.g. E:/mayadev/devkitBase)")

if(NOT MAYA_LOCATION)
  message(FATAL_ERROR "MAYA_LOCATION is empty. Pass -DMAYA_LOCATION=...")
endif()
if(NOT DEVKIT_BASE)
  message(FATAL_ERROR "DEVKIT_BASE is empty. Pass -DDEVKIT_BASE=...")
endif()

# Basic sanity checks
if(NOT EXISTS "${MAYA_LOCATION}/include/maya/MFnPlugin.h")
  message(FATAL_ERROR "Maya headers not found: ${MAYA_LOCATION}/include/maya/MFnPlugin.h")
endif()
if(NOT EXISTS "${DEVKIT_BASE}/include/maya/MHUDRender.h")
  message(FATAL_ERROR "DevKit headers not found: ${DEVKIT_BASE}/include/maya/MHUDRender.h")
endif()

# ---- Target ---------------------------------------------------------------
add_library(ao_frameguide MODULE
  src/ao_frameguide.cpp
)

target_compile_features(ao_frameguide PRIVATE cxx_std_17)

# Includes: Maya本体 + devkitBase（あなたの環境）
target_include_directories(ao_frameguide PRIVATE
  "${MAYA_LOCATION}/include"
  "${DEVKIT_BASE}/include"
)

# Lib paths: Maya本体 + devkitBase
target_link_directories(ao_frameguide PRIVATE
  "${MAYA_LOCATION}/lib"
  "${DEVKIT_BASE}/lib"
)

# VP2 RenderOverride系でよく必要になる libs
target_link_libraries(ao_frameguide PRIVATE
  Foundation
  OpenMaya
  OpenMayaUI
  OpenMayaRender
)

# Windows / Maya plugin defines
target_compile_definitions(ao_frameguide PRIVATE
  NT_PLUGIN
  REQUIRE_IOSTREAM
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  # 文字化け(C4819)回避の定番
  target_compile_options(ao_frameguide PRIVATE /EHsc /utf-8 /Zc:__cplusplus)
endif()

# Maya plugin naming on Windows
set_target_properties(ao_frameguide PROPERTIES
  PREFIX ""
  SUFFIX ".mll"
)

# Output to build/bin/<config>/
set_target_properties(ao_frameguide PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${cfg}" CFGU)
  set_target_properties(ao_frameguide PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}"
    LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}"
    ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/lib/${cfg}"
  )
endforeach()
