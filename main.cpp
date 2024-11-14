#include "IconsFontAwesome4.h"

#include <cmath>
#include <csignal>
#include <functional>
#include <vector>
#include <print>

// #define IMGUI_ENABLE_FREETYPE
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#include "nodes_panel.h"
#include "fst_file.h"
#include "waveform_viewer.h"

#include <pybind11/embed.h>
namespace py = pybind11;

static void glfw_error_callback(int error, const char * description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void signalHandler(int signum) { exit(signum); }

// Main code
int main(int, char **) {
    signal(SIGINT, signalHandler);
    py::scoped_interpreter guard{}; // start the interpreter and keep it alive

    auto module          = py::module::import("read_stuff");
    auto mesh_viz_module = py::module::import("mesh_viz");
    auto imgui_module    = py::module::import("imgui");

    auto process_func = module.attr("process");

    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit()) return 1;

        // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char * glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char * glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char * glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    float xscale, yscale;

    // Create window with graphics context
    GLFWwindow * window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if(window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // glfwGetWindowContentScale(window, &xscale, &yscale);
    // DPI_SCALE = xscale;
    // std::println("xscale = {}, yscale = {}", xscale, yscale);
    glfwSetWindowContentScaleCallback(window, [](GLFWwindow * window, float xscale, float yscale) {
        std::println("xscale = {}, yscale = {}", xscale, yscale);
        DPI_SCALE = xscale;
    });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.Fonts->AddFontFromFileTTF("../NotoSans[wdth,wght].ttf", 22);
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig         icons_config;
    icons_config.MergeMode  = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("../fontawesome-webfont.ttf", 22.0f, &icons_config, icons_ranges);

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    FstFile        f("../../toplevel/out.fst");
    WaveformViewer waveform_viewer(&f);
    NodesPanel     panel(f.read_nodes(), &waveform_viewer);

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport * viewport = ImGui::GetMainViewport();

        {
            auto dockid = ImGui::DockSpaceOverViewport();

            auto current_time = waveform_viewer.render();

            ImGui::Begin(
                "Mesh", nullptr,
                ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse); // Create a window called "Hello, world!" and append into it.
            if(ImGui::IsKeyPressed(ImGuiKey_R)) {
                try {
                    module.reload();
                    process_func = module.attr("process");
                } catch(...) {}
            }

            ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
            ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

            ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

            panel.render(current_time, canvas_p0, canvas_p1, process_func);

            ImGui::End();

            // auto [current_time, min_time, max_time] = timeline.render();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w,
                     clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
