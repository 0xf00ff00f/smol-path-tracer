cmake_minimum_required(VERSION 3.28)
project(smol_path_tracer)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_program(GLSLC glslc REQUIRED)

add_library(gpu_module)
target_sources(gpu_module PUBLIC FILE_SET CXX_MODULES FILES gpu.cpp)
target_compile_options(gpu_module PRIVATE -freflection-latest)
target_link_libraries(gpu_module PRIVATE Vulkan::Vulkan)

add_executable(simple main.cpp)
target_compile_options(simple PRIVATE -freflection-latest)
target_link_libraries(simple PRIVATE gpu_module Vulkan::Vulkan)
