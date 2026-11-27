cmake_minimum_required(VERSION 3.28)
project(smol_path_tracer)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

find_path(RENDERDOC_INCLUDE_DIR "renderdoc_app.h" HINTS "$ENV{HOME}/opt/renderdoc/include")
message(STATUS "RENDERDOC_INCLUDE_DIR=${RENDERDOC_INCLUDE_DIR}")
if (RENDERDOC_INCLUDE_DIR STREQUAL "RENDERDOC_INCLUDE_DIR-NOTFOUND")
    set(USE_RENDERDOC FALSE)
else()
    set(USE_RENDERDOC TRUE)
endif()

add_library(gpu)
target_sources(gpu PUBLIC FILE_SET CXX_MODULES FILES gpu.cpp)
target_link_libraries(gpu PRIVATE Vulkan::Vulkan)

add_library(vecmath)
target_sources(vecmath PUBLIC FILE_SET CXX_MODULES FILES vecmath.cpp)

if (USE_RENDERDOC)
    add_library(renderdoc)
    target_sources(renderdoc PUBLIC FILE_SET CXX_MODULES FILES renderdoc.cpp)
    target_include_directories(renderdoc PRIVATE ${RENDERDOC_INCLUDE_DIR})
endif()

macro(AddDemo)
  set(options)
  set(oneValueArgs NAME)
  set(multiValueArgs SOURCES SHADERS)

  cmake_parse_arguments(DEMO "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  add_executable(${DEMO_NAME} ${DEMO_SOURCES})
  target_link_libraries(${DEMO_NAME} PRIVATE gpu vecmath)
  if (USE_RENDERDOC)
      target_link_libraries(${DEMO_NAME} PRIVATE renderdoc)
      target_compile_definitions(${DEMO_NAME} PRIVATE USE_RENDERDOC)
  endif()

  foreach(SHADER ${DEMO_SHADERS})
    set(SHADER_BINARY ${CMAKE_BINARY_DIR}/${SHADER}.spv)
    add_custom_command(
      OUTPUT ${SHADER_BINARY}
      COMMAND ${GLSLANG_VALIDATOR} -gVS -V ${CMAKE_SOURCE_DIR}/${SHADER} -o
              ${SHADER_BINARY}
      DEPENDS ${PROJECT_SOURCE_DIR}/${SHADER})
    list(APPEND ${DEMO_NAME}_SHADER_BINARIES ${SHADER_BINARY})
  endforeach(SHADER)
  add_custom_target(${DEMO_NAME}_compile_shaders
                    DEPENDS ${${DEMO_NAME}_SHADER_BINARIES})
  add_dependencies(${DEMO_NAME} ${DEMO_NAME}_compile_shaders)
endmacro()

AddDemo(NAME simple SOURCES simple.cpp SHADERS simple.comp)
AddDemo(NAME raycast SOURCES raycast.cpp SHADERS raycast.comp)
AddDemo(NAME pathtrace SOURCES pathtrace.cpp SHADERS pathtrace.comp)
