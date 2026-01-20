cmake_minimum_required(VERSION 3.20)

project(ao_viewport_guide VERSION 0.1.0 LANGUAGES CXX)

# ---- C++ settings ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Maya location (must be passed) ----
# Example:
#   -DMAYA_LOCATION="C:/Program Files/Autodesk/Maya2025"
if(NOT MAYA_LOCATION)
  message(FATAL_ERROR "MAYA_LOCATION is not set. Example: -DMAYA_LOCATION=\"C:/Program Files/Autodesk/Maya2025\"")
endif()

set(MAYA_INCLUDE_DIR "${MAYA_LOCATION}/include")
set(MAYA_LIBRARY_DIR "${MAYA_LOCATION}/lib")

if(NOT EXISTS "${MAYA_INCLUDE_DIR}")
  message(FATAL_ERROR "Maya include dir not found: ${MAYA_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${MAYA_LIBRARY_DIR}")
  message(FATAL_ERROR "Maya lib dir not found: ${MAYA_LIBRARY_DIR}")
endif()

# ---- Plugin target ----
set(PLUGIN_NAME "ao_viewport_guide")

add_library(${PLUGIN_NAME} SHARED
  src/pluginMain.cpp
)

target_include_directories(${PLUGIN_NAME} PRIVATE "${MAYA_INCLUDE_DIR}")
target_link_directories(${PLUGIN_NAME} PRIVATE "${MAYA_LIBRARY_DIR}")

# Maya SDK libs (minimum for command + HUD)
target_link_libraries(${PLUGIN_NAME} PRIVATE
  Foundation
  OpenMaya
  OpenMayaUI
  OpenMayaAnim
)

# Windows: .mll
if(WIN32)
  set_target_properties(${PLUGIN_NAME} PROPERTIES SUFFIX ".mll")
  target_compile_definitions(${PLUGIN_NAME} PRIVATE
    NT_PLUGIN
    REQUIRE_IOSTREAM
    _USE_MATH_DEFINES
  )
  target_compile_options(${PLUGIN_NAME} PRIVATE /EHsc)
endif()

# Output folder
set_target_properties(${PLUGIN_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
