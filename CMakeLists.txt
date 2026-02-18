cmake_minimum_required(VERSION 3.20)
project(ao_viewport_guide LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set this from command line:
# -DMAYA_LOCATION="C:/Program Files/Autodesk/Maya2025"
set(MAYA_LOCATION "" CACHE PATH "Maya install root")

if (NOT MAYA_LOCATION)
  message(FATAL_ERROR "MAYA_LOCATION is required. e.g. -DMAYA_LOCATION=\"C:/Program Files/Autodesk/Maya2025\"")
endif()

set(MAYA_INCLUDE_DIR "${MAYA_LOCATION}/include")
set(MAYA_LIB_DIR     "${MAYA_LOCATION}/lib")

add_library(${PROJECT_NAME} SHARED
  src/aoViewportGuidePlugin.cpp
  src/aoViewportGuideOverride.cpp
  src/aoViewportGuideGate.cpp
  src/aoViewportGuideSettings.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${MAYA_INCLUDE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  NT_PLUGIN
  REQUIRE_IOSTREAM
  NOMINMAX
)

if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /EHsc /utf-8)
endif()

target_link_directories(${PROJECT_NAME} PRIVATE "${MAYA_LIB_DIR}")

target_link_libraries(${PROJECT_NAME} PRIVATE
  Foundation
  OpenMaya
  OpenMayaUI
  OpenMayaRender
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  SUFFIX ".mll"
)

# Output to build/dist/<Config>
set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/$<CONFIG>"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/$<CONFIG>"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/$<CONFIG>"
)
