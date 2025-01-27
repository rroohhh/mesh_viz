cmake_minimum_required(VERSION 3.15...3.29)
project(mesh_viz LANGUAGES CXX C ASM)

find_package(pybind11 CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED)

find_package(folly CONFIG REQUIRED SHARED)
find_package(Gflags CONFIG REQUIRED)
set_target_properties(gflags_shared PROPERTIES
        MAP_IMPORTED_CONFIG_DEBUG Release
)


set (CMAKE_EXPORT_COMPILE_COMMANDS 1)

set (EXECUTABLE_OPT_FILES imgui/imgui.cpp imgui/imgui_demo.cpp  imgui/imgui_widgets.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp pybind_imgui.cpp formatter.cpp waveform_viewer.cpp node.cpp bind.cpp nodes_panel.cpp core.cpp fst_file.cpp wave_data_base.cpp implot/implot.cpp implot/implot_items.cpp histogram.cpp inverted_index.cpp ../toplevel/mesh_utils.cpp highlights.cpp node_var.cpp fst_reader.cpp maskedvbyte/src/varintdecode.c)
set (EXECUTABLE_FILES main.cpp fonts.s ${EXECUTABLE_OPT_FILES})
set_source_files_properties(fonts.s OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/NotoSans[wdth,wght].ttf;${CMAKE_SOURCE_DIR}/fontawesome-webfont.ttf"
)

set_source_files_properties(fonts.s OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/NotoSans[wdth,wght].ttf;${CMAKE_SOURCE_DIR}/fontawesome-webfont.ttf"
)

add_executable(mesh_viz ${EXECUTABLE_FILES})

set (OPT_FILES libfst/fstapi.c libfst/lz4.c libfst/fastlz.c imgui/misc/cpp/imgui_stdlib.h)
target_sources(mesh_viz PRIVATE ${OPT_FILES})

# set_source_files_properties(${EXECUTABLE_OPT_FILES} ${OPT_FILES} PROPERTIES COMPILE_FLAGS "-O3 -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")


set(IMPLOT_OPTIONS "-DIMPLOT_CUSTOM_NUMERIC_TYPES=(signed char)(unsigned char)(signed short)(unsigned short)(signed int)(unsigned int)(signed long)(unsigned long)(signed long long)(unsigned long long)(float)(double)(long double)")

target_compile_options(mesh_viz
  PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -std=c++23 -O3 -fno-math-errno -ggdb -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra ${IMPLOT_OPTIONS} -fvisibility=hidden>
)

target_compile_options(mesh_viz
  PRIVATE $<$<COMPILE_LANGUAGE:C>: -DFST_CONFIG_INCLUDE=\"fstapi.h\"  -O3 -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra >
)

target_compile_options(mesh_viz PRIVATE -DMYPYBIND11_MODULE=PYBIND11_EMBEDDED_MODULE)
target_link_libraries(mesh_viz PRIVATE pybind11::embed glfw z GL ${Boost_LIBRARIES} Folly::folly)

# include CMAKE_SOURCE_DIR for fonts.s to find the font files
target_include_directories(mesh_viz PRIVATE ${CMAKE_SOURCE_DIR} imgui imgui/backends implot ../toplevel ${Boost_INCLUDE_DIRS} ${FOLLY_INCLUDE_DIRS} maskedvbyte/include)

target_compile_options(mesh_viz PRIVATE -fmax-errors=1)
# target_compile_options(mesh_viz PRIVATE -flto)
# target_link_options(mesh_viz PRIVATE -flto)

# target_compile_options(mesh_viz PRIVATE -fsanitize=address)
# target_link_options(mesh_viz PRIVATE -fsanitize=address)




add_library(mesh_viz_lib MODULE ${EXECUTABLE_FILES})
target_link_libraries(mesh_viz_lib PUBLIC pybind11::module)
target_compile_options(mesh_viz_lib
  PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -std=c++23 -O3 -fno-math-errno -ggdb -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra ${IMPLOT_OPTIONS} -fvisibility=hidden>
)

target_compile_options(mesh_viz_lib
  PRIVATE $<$<COMPILE_LANGUAGE:C>: -DFST_CONFIG_INCLUDE=\"fstapi.h\"  -O3 -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra >
)

target_link_libraries(mesh_viz_lib PRIVATE glfw z GL ${Boost_LIBRARIES} Folly::folly -static-libgcc -static-libstdc++)

# include CMAKE_SOURCE_DIR for fonts.s to find the font files
target_include_directories(mesh_viz_lib PRIVATE ${CMAKE_SOURCE_DIR} imgui imgui/backends implot ../toplevel ${Boost_INCLUDE_DIRS} ${FOLLY_INCLUDE_DIRS} maskedvbyte/include)


target_compile_options(mesh_viz_lib PRIVATE -fmax-errors=1)
target_compile_options(mesh_viz_lib PRIVATE -DMYPYBIND11_MODULE=PYBIND11_MODULE)
target_sources(mesh_viz_lib PRIVATE ${OPT_FILES})


add_executable(bench_db bench_db.cpp)
target_sources(bench_db PRIVATE wave_data_base.cpp)

# -ggdb
target_compile_options(bench_db PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -std=c++23 -O3 -march=native -mtune=native -fdiagnostics-color=always -Wall -Wextra>)

# target_compile_options(bench_db PRIVATE -fsanitize=address)
# target_link_options(bench_db PRIVATE -fsanitize=address)


# workaround clang++ somehow not finding this one correctly
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold -Wl,-rpath=/home/robin/.guix-home/profile/lib/")
target_link_libraries(bench_db PRIVATE Folly::folly /home/robin/.guix-home/profile/lib/libstdc++.so)
