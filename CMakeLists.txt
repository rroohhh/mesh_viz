cmake_minimum_required(VERSION 3.15...3.29)
project(mesh_viz LANGUAGES CXX C)

find_package(pybind11 CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED)

find_package(folly CONFIG REQUIRED SHARED)
find_package(Gflags CONFIG REQUIRED)
set_target_properties(gflags_shared PROPERTIES
        MAP_IMPORTED_CONFIG_DEBUG Release
)


set (CMAKE_EXPORT_COMPILE_COMMANDS 1)

set (EXECUTABLE_OPT_FILES imgui/imgui.cpp imgui/imgui_demo.cpp  imgui/imgui_widgets.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp pybind_imgui.cpp formatter.cpp waveform_viewer.cpp node.cpp bind.cpp nodes_panel.cpp core.cpp fst_file.cpp wave_data_base.cpp)
set (EXECUTABLE_FILES main.cpp ${EXECUTABLE_OPT_FILES})
add_executable(mesh_viz ${EXECUTABLE_FILES})

set (OPT_FILES libfst/fstapi.c libfst/lz4.c libfst/fastlz.c imgui/misc/cpp/imgui_stdlib.h)
target_sources(mesh_viz PRIVATE ${OPT_FILES})

# set_source_files_properties(${EXECUTABLE_OPT_FILES} ${OPT_FILES} PROPERTIES COMPILE_FLAGS "-O3 -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")

target_compile_options(mesh_viz
  PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -std=c++23 -Og -ggdb -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra>
)

target_compile_options(mesh_viz
  PRIVATE $<$<COMPILE_LANGUAGE:C>: -DFST_CONFIG_INCLUDE=\"fstapi.h\"  -O3 -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra >
)

target_include_directories(mesh_viz PRIVATE imgui imgui/backends ../toplevel  ${Boost_INCLUDE_DIRS})

target_link_libraries(mesh_viz PRIVATE pybind11::embed c++ glfw z GL ${Boost_LIBRARIES} Folly::folly)


add_executable(bench_db bench_db.cpp)
target_sources(mesh_viz PRIVATE wave_data_base.cpp)

# -ggdb
target_compile_options(bench_db PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -std=c++23 -O3 -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra>)

# target_compile_options(bench_db PRIVATE -fsanitize=address)
# target_link_options(bench_db PRIVATE -fsanitize=address)


# workaround clang++ somehow not finding this one correctly
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -Wl,-rpath=/home/robin/.guix-home/profile/lib/")
target_link_libraries(bench_db PRIVATE Folly::folly /home/robin/.guix-home/profile/lib/libstdc++.so)
