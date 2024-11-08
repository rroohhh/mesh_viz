cmake_minimum_required(VERSION 3.15...3.29)
project(mesh_viz LANGUAGES CXX C)

find_package(pybind11 CONFIG REQUIRED)

set (CMAKE_EXPORT_COMPILE_COMMANDS 1)

set (EXECUTABLE_OPT_FILES imgui/imgui.cpp imgui/imgui_demo.cpp  imgui/imgui_widgets.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp pybind_imgui.cpp)
set (EXECUTABLE_FILES main.cpp ${EXECUTABLE_OPT_FILES})
add_executable(mesh_viz ${EXECUTABLE_FILES})

set (OPT_FILES libfst/fstapi.c libfst/lz4.c libfst/fastlz.c imgui/misc/cpp/imgui_stdlib.h)
target_sources(mesh_viz PRIVATE ${OPT_FILES})

set_source_files_properties(${EXECUTABLE_OPT_FILES} ${OPT_FILES} PROPERTIES COMPILE_FLAGS -O3 -march=native -mtune=native)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")

target_compile_options(mesh_viz
  PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -std=c++23 -stdlib=libc++>
)

target_compile_options(mesh_viz
  PRIVATE $<$<COMPILE_LANGUAGE:C>: -DFST_CONFIG_INCLUDE=\"fstapi.h\" >
)

target_include_directories(mesh_viz PRIVATE imgui imgui/backends ../toplevel /home/robin/.guix-home/profile/include/c++/v1 /home/robin/.guix-home/profile/include/)

target_link_libraries(mesh_viz PRIVATE pybind11::embed c++ glfw z GL)
