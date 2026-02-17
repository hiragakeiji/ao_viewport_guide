cmake_minimum_required(VERSION 3.20)
project(ao_viewport_guide LANGUAGES CXX)

# ---- user inputs ----
set(MAYA_LOCATION "" CACHE PATH "Path to Maya install, e.g. C:/Program Files/Autodesk/Maya2025")
set(DEVKIT_BASE   "" CACHE PATH "Optional: path to your devkitBase (if you use it)")

if (NOT EXISTS "${MAYA_LOCATION}/include/maya/MFnPlugin.h")
  message(FATAL_ERROR "MAYA_LOCATION is invalid or missing headers: ${MAYA_LOCATION}/include")
endif()

# ---- target ----
add_library(ao_viewport_guide MODULE
  src/aoViewportGuidePlugin.cpp
  src/aoViewportGuideOverride.cpp
  src/aoViewportGuideGate.cpp
  src/aoViewportGuideSettings.cpp
)

target_compile_features(ao_viewport_guide PRIVATE cxx_std_17)

# include dirs
target_include_directories(ao_viewport_guide PRIVATE
  "${MAYA_LOCATION}/include"
)

if (DEVKIT_BASE AND EXISTS "${DEVKIT_BASE}/include/maya/MFnPlugin.h")
  target_include_directories(ao_viewport_guide PRIVATE "${DEVKIT_BASE}/include")
endif()

# Windows defines (Maya plugin)
if (WIN32)
  target_compile_definitions(ao_viewport_guide PRIVATE
    NT_PLUGIN
    REQUIRE_IOSTREAM
    _CRT_SECURE_NO_WARNINGS
  )
endif()

# link libs (Maya)
if (WIN32)
  target_link_directories(ao_viewport_guide PRIVATE "${MAYA_LOCATION}/lib")
  target_link_libraries(ao_viewport_guide PRIVATE
    OpenMaya
    OpenMayaRender
    Foundation
  )
endif()

# output as .mll (no "lib" prefix)
set_target_properties(ao_viewport_guide PROPERTIES
  PREFIX ""
  SUFFIX ".mll"
)

# output folders
set_target_properties(ao_viewport_guide PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/$<CONFIG>"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/$<CONFIG>"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/$<CONFIG>"
)
