cmake_minimum_required(VERSION 3.28)
project(smol_path_tracer)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

add_library(gpu_module)
target_sources(gpu_module PUBLIC FILE_SET CXX_MODULES FILES gpu.cpp)
target_compile_options(gpu_module PRIVATE -freflection-latest)
target_link_libraries(gpu_module PRIVATE Vulkan::Vulkan)

macro(AddDemo)
  set(options)
  set(oneValueArgs NAME)
  set(multiValueArgs SOURCES SHADERS)

  cmake_parse_arguments(DEMO "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  add_executable(${DEMO_NAME} ${DEMO_SOURCES})
  target_compile_options(${DEMO_NAME} PRIVATE -freflection-latest)
  target_link_libraries(${DEMO_NAME} PRIVATE gpu_module)

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
