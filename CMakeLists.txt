cmake_minimum_required(VERSION 3.20)

project(ao_viewport_guide VERSION 0.1.0 LANGUAGES CXX)

# ---- User settings ----
set(MAYA_LOCATION "" CACHE PATH "Path to Autodesk Maya (e.g. C:/Program Files/Autodesk/Maya2025)")
set(DEVKIT_BASE   "" CACHE PATH "Optional: Path to devkitBase (e.g. E:/mayadev/devkitBase)")

if(NOT MAYA_LOCATION)
  message(FATAL_ERROR "Please set MAYA_LOCATION. Example: -DMAYA_LOCATION=\"C:/Program Files/Autodesk/Maya2025\"")
endif()

set(MAYA_INCLUDE_DIR "${MAYA_LOCATION}/include")
set(MAYA_LIB_DIR     "${MAYA_LOCATION}/lib")

# ---- Target ----
add_library(${PROJECT_NAME} SHARED
  src/aoViewportGuidePlugin.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Maya headers
target_include_directories(${PROJECT_NAME} PRIVATE
  "${MAYA_INCLUDE_DIR}"
)

# Optional devkitBase headers (if you have extra headers there)
if(DEVKIT_BASE AND EXISTS "${DEVKIT_BASE}/include")
  target_include_directories(${PROJECT_NAME} PRIVATE
    "${DEVKIT_BASE}/include"
  )
endif()

# Link Maya libs
target_link_directories(${PROJECT_NAME} PRIVATE
  "${MAYA_LIB_DIR}"
)

# Typical libs for VP2 override + node
target_link_libraries(${PROJECT_NAME} PRIVATE
  Foundation
  OpenMaya
  OpenMayaUI
  OpenMayaRender
)

# Windows plugin settings
if(WIN32)
  # Maya plugin extension
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".mll")

  # Common defines for Maya plugins on Windows
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    NT_PLUGIN
    REQUIRE_IOSTREAM
  )

  # (optional) put the built .mll in build/bin
  set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endif()
