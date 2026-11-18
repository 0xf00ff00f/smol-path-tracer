cmake_minimum_required(VERSION 3.25)
project(smol_path_tracer)

find_package(Vulkan REQUIRED)

add_executable(spt main.cpp)
set_target_properties(spt PROPERTIES CXX_STANDARD 26)
target_link_libraries(spt Vulkan::Vulkan)
