#include "mapper.hpp"

#include <SDL3/SDL_hints.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#include <SDL3/SDL_opengl.h>
#pragma clang diagnostic pop

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

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

SDL_Window* window = {};
SDL_GLContext context = {};

void OpenGUI()
{
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_EnableScreenSaver();

    window = SDL_CreateWindow("Mapper", 960, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    // Init OpenGL

    context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(0);

    // Init ImGui

    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL3_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init();

    SDL_AddEventWatch([](void *, SDL_Event *event) -> bool {
        ImGui_ImplSDL3_ProcessEvent(event);
        return true;
    }, nullptr);
}

// -----------------------------------------------------------------------------

template<typename MenuFn>
static
void BeginFrame(MenuFn&& menu_fn)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
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
    SDL_GetWindowSizeInPixels(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.2, 0.2, 0.2, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
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
            }

            for (uint32_t i = 0; i < vjoy->num_axes; ++i) {
                float v = vjoy->GetAxis(i);
                if (ImGui::SliderFloat(std::format("{}##axis", i).c_str(), &v, -1.f, 1.f)) {
                    vjoy->SetAxis(i, v);
                }
            }

            for (uint32_t i = 0; i < vjoy->num_buttons; ++i) {
                bool pressed = vjoy->GetButton(i);

                if (i > 0 && i % 8) ImGui::SameLine(0.f, ImGui_ToggleButtonSpacing);
                if (ImGui_ToggleButton(std::format("{}##button", i).c_str(), {30, 30}, &pressed, true)) {
                    vjoy->SetButton(i, pressed);
                }
            }
        }
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

    ImGui_Print("Frame: {}", frame);
    ImGui_Print("Script Time: {} ({:.3f}%)", DurationToString(average_script_dur), average_script_util * 100.f);
}

void DrawGUI()
{
    BeginFrame([] {
        DrawFileMenu();
    });
    DrawLoadedScriptPanel();
    DrawVirtualJoysticksPanel();
    DrawJoystickInputViewer();
    DrawStatsPanel();
    EndFrame();
}
