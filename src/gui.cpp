#include "mapper.hpp"

#include <GLFW/glfw3.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#include <SDL3/SDL_opengl.h>
#pragma clang diagnostic pop

#if defined(WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")
#endif

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <thread>

#define ImGui_Print(...) ImGui::Text("%s", std::format(__VA_ARGS__).c_str())

constexpr auto ImGui_ToggleButtonSpacing = 4.f;
constexpr auto ImGui_TopWindowPaddingAdjustment = -3.f;

static
bool ImGui_ToggleButton(const char* message, ImVec2 size, bool* pressed, bool user_input)
{
    auto color = ImGui::GetStyleColorVec4(*pressed ? ImGuiCol_ButtonActive : ImGuiCol_Button);
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    if (!user_input) {
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    }
    bool interacted = false;
    if (ImGui::Button(message, size) && user_input) {
        *pressed = !*pressed;
        interacted = true;
    }
    ImGui::PopStyleColor(user_input ? 1 : 3);
    return interacted;
};

struct ImGui_IDGuard
{
    template<typename T>
    ImGui_IDGuard(T t)
    {
        ImGui::PushID(t);
    }

    ~ImGui_IDGuard()
    {
        ImGui::PopID();
    }
};

GLFWwindow* window;
std::jthread gui_thread;
std::atomic<uint64_t> pending_gui_update_id = 1;

void PushGUIRedrawEvent()
{
    ++pending_gui_update_id;
    glfwPostEmptyEvent();
}

void DrawGUI();

void OpenGUI()
{
    gui_thread = std::jthread([] {
        glfwInit();

        glfwWindowHintString(GLFW_WAYLAND_APP_ID, "Mapper");

        window = glfwCreateWindow(960, 720, "Mapper", nullptr, nullptr);

#if defined(WIN32)
        {
            auto hwnd = glfwGetWin32Window(window);
            BOOL value = true;
            ::DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &value, sizeof(value));
        }
#endif

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui_ImplGlfw_InitForOpenGL(window, false);
        ImGui_ImplOpenGL3_Init();

        // Only redraw GUI on specific events to filter out Wayland refresh events

        #define MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(name, ...) glfwSet##name(__VA_ARGS__ __VA_OPT__(,) [](auto... args) { \
            ++pending_gui_update_id; \
            ImGui_ImplGlfw_##name(args...); \
        })

        #define MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(name, ...) glfwSet##name(__VA_ARGS__ __VA_OPT__(,) [](auto...) { \
            ++pending_gui_update_id; \
        })

        MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(WindowFocusCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(CursorEnterCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(CursorPosCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(MouseButtonCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(ScrollCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(KeyCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(CharCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_PASSTHROUGH_IMGUI(MonitorCallback);

        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(WindowPosCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(WindowSizeCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(WindowCloseCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(WindowRefreshCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(WindowIconifyCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(WindowMaximizeCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(FramebufferSizeCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(WindowContentScaleCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(CharModsCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(DropCallback, window);
        MAPPER_REGISTER_GLFW_CALLBACK_EMPTY(JoystickCallback);

        uint64_t gui_update_id = 0;

        while (!glfwWindowShouldClose(window)) {
            auto next_gui_update_id = pending_gui_update_id.load();
            if (next_gui_update_id > gui_update_id) {
                gui_update_id = next_gui_update_id;
                DrawGUI();
            }

            glfwWaitEvents();
        }

        glfwTerminate();
    });
}

void CloseGUI()
{
    ++pending_gui_update_id;
    glfwSetWindowShouldClose(window, true);
    glfwPostEmptyEvent();
    if (gui_thread.joinable()) {
        gui_thread.join();
    }
}

// -----------------------------------------------------------------------------

template<typename MenuFn>
static
void BeginFrame(MenuFn&& menu_fn)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Dockspace

    ImGuiDockNodeFlags dockspace_flags = 0;
    ImGuiWindowFlags dockspace_window_flags = 0;

    // Make dockspace fullscreen
    auto viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    dockspace_window_flags
        |= ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_MenuBar;

    // Register dockspace
    bool show = true;
    ImGui::Begin("Dockspace", &show, dockspace_window_flags);
    ImGui::PopStyleVar(3);
    ImGui::DockSpace(ImGui::GetID("DockspaceID"), ImVec2(0.f, 0.f), dockspace_flags);

    ImGui::BeginMenuBar();
    menu_fn();
    ImGui::EndMenuBar();

    ImGui::End();
}

static
void EndFrame()
{
    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.2, 0.2, 0.2, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

static
void DrawFileMenu()
{
    if (!ImGui::BeginMenu("File")) return;

    if (ImGui::MenuItem("Load script")) {
        SDL_ShowOpenFileDialog([](void* /* userdata */, const char* const* filelist, int /* filter */) {
            if (!filelist) {
                Log("Error selecting file: {}", SDL_GetError());
                return;
            }

            std::scoped_lock _{ engine_mutex };

            for (const char* file; (file = *filelist); ++filelist) {
                Log("Loading script: {}", file);
                auto path = std::filesystem::canonical(file);
                LoadScript(file);
            }
        }, nullptr, nullptr, nullptr, 0, std::filesystem::current_path().string().c_str(), true);
    }

    ImGui::EndMenu();
}

static
void DrawLoadedScriptPanel()
{
    Defer _ = [] { ImGui::End(); };
    if (!ImGui::Begin("Scripts")) return;

    for (auto& script : scripts) {
        ImGui_IDGuard _ = script;
        {
            if (script->disabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32({ 1.f, 0.f, 0.f, 1.f }));
            Defer _ = [&] { if (script->disabled) ImGui::PopStyleColor(); };
            if (!ImGui::CollapsingHeader(std::format("{}{}###", script->path.string(), script->disabled ? " (DISABLED)" : "").c_str())) continue;
        }

        if (ImGui::Button("Unload")) {
            QueueUnloadScript(script);
        }

        ImGui::SameLine();

        if (ImGui::Button("Reload")) {
            LoadScript(script);
        }

        if (script->disabled) {
            ImGui_Print("Disabled, reason:");
            ImGui::TextWrapped("%s", script->error.c_str());
        }
    }

    FlushScriptDeleteQueue();
}

static
void DrawVirtualJoysticksPanel()
{
    Defer _ = [] { ImGui::End(); };
    if (!ImGui::Begin("Virtual Joysticks")) return;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui_TopWindowPaddingAdjustment);

    bool any_change = false;

    for (auto* script : scripts) {
        for (auto* vjoy : script->vjoysticks) {
            ImGui_IDGuard _ = vjoy;

            if (!ImGui::CollapsingHeader(std::format("{}", vjoy->name).c_str())) continue;

            if (ImGui::Button("Reset", ImVec2(100, 0))) {
                for (uint32_t i = 0; i < vjoy->num_axes; ++i) {
                    vjoy->SetAxis(i, 0.f);
                }
                for (uint32_t i = 0; i < vjoy->num_buttons; ++i) {
                    vjoy->SetButton(i, false);
                }
                any_change = true;
            }

            for (uint32_t i = 0; i < vjoy->num_axes; ++i) {
                float v = vjoy->GetAxis(i);
                if (ImGui::SliderFloat(std::format("{}##axis", i).c_str(), &v, -1.f, 1.f)) {
                    vjoy->SetAxis(i, v);
                    any_change = true;
                }
            }

            for (uint32_t i = 0; i < vjoy->num_buttons; ++i) {
                bool pressed = vjoy->GetButton(i);

                if (i > 0 && i % 8) ImGui::SameLine(0.f, ImGui_ToggleButtonSpacing);
                if (ImGui_ToggleButton(std::format("{}##button", i).c_str(), {30, 30}, &pressed, true)) {
                    vjoy->SetButton(i, pressed);
                    any_change = true;
                }
            }
        }
    }

    if (any_change) {
        PushJoystickUpdateEvent();
    }
}

static
void DrawJoystickInputViewer()
{
    Defer _ = [] { ImGui::End(); };
    if (!ImGui::Begin("Joystick Input Viewer")) return;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui_TopWindowPaddingAdjustment);

    for (auto joystick : joysticks) {
        auto name = SDL_GetJoystickName(joystick);
        auto vendor_id = SDL_GetJoystickVendor(joystick);
        auto product_id = SDL_GetJoystickProduct(joystick);
        auto version = SDL_GetJoystickProductVersion(joystick);

        auto guid = SDL_GetJoystickGUID(joystick);
        /* SDL_GUID : (bus_type, 0, vendor_id, 0, product_id, 0, version, 0) */
        uint16_t bus_type;
        std::memcpy(&bus_type, guid.data, 2);

        ImGui_IDGuard _ = joystick;

        if (!ImGui::CollapsingHeader(std::format("({:#06x}/{:#06x}/{:#06x}/{:#06x}) {}", bus_type, vendor_id, product_id, version, name).c_str())) continue;

        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        for (int i = 0; i < SDL_GetNumJoystickAxes(joystick); ++i) {
            auto raw = SDL_GetJoystickAxis(joystick, i);

            auto normalized = FromSNorm(raw);
            ImGui::SliderFloat(std::format("{} ({})###axis.{}", i, raw, i).c_str(), &normalized, -1.f, 1.f, "%.3f", ImGuiSliderFlags_NoInput);
        }
        ImGui::PopItemFlag();

        for (int i = 0; i < SDL_GetNumJoystickButtons(joystick); ++i) {
            auto pressed = SDL_GetJoystickButton(joystick, i);

            if (i > 0 && i % 8) ImGui::SameLine(0.f, ImGui_ToggleButtonSpacing);
            ImGui_ToggleButton(std::format("{}##button", i).c_str(), {30, 30}, &pressed, false);
        }

        for (int i = 0; i < SDL_GetNumJoystickHats(joystick); ++i) {
            auto hat = SDL_GetJoystickHat(joystick, i);

            const char* hat_str = "?";
            switch (hat) {
                case SDL_HAT_CENTERED: hat_str = "CENTERED"; break;
                case SDL_HAT_UP: hat_str = "UP"; break;
                case SDL_HAT_RIGHT: hat_str = "RIGHT"; break;
                case SDL_HAT_DOWN: hat_str = "DOWN"; break;
                case SDL_HAT_LEFT: hat_str = "LEFT"; break;
                case SDL_HAT_RIGHTUP: hat_str = "RIGHTUP"; break;
                case SDL_HAT_RIGHTDOWN: hat_str = "RIGHTDOWN"; break;
                case SDL_HAT_LEFTUP: hat_str = "LEFTUP"; break;
                case SDL_HAT_LEFTDOWN: hat_str = "LEFTDOWN"; break;
            }

            ImGui_Print("Hat[{}] = {}", i, hat_str);
        }
    }
}

static
void DrawStatsPanel()
{
    Defer _ = [] { ImGui::End(); };
    if (!ImGui::Begin("Stats")) return;

    ImGui_Print("Joystick Updates: {}", frame);
    ImGui_Print("GUI Frames: {}", gui_frame);
    ImGui_Print("Script Time: {} ({:.3f}%)", DurationToString(average_script_dur), average_script_util * 100.f);
}

void DrawGUI()
{
    ++gui_frame;
    BeginFrame([] {
        DrawFileMenu();
    });
    {
        std::scoped_lock _{ engine_mutex };
        DrawLoadedScriptPanel();
        DrawVirtualJoysticksPanel();
        DrawJoystickInputViewer();
        DrawStatsPanel();
    }
    EndFrame();
}
