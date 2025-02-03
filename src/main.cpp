#include <mapper.hpp>

#include <sol/sol.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#include <SDL3/SDL_opengl.h>
#pragma clang diagnostic pop

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include <cmath>
#include <format>
#include <chrono>
#include <filesystem>
#include <unordered_set>

#include "vjoystick.hpp"

// -----------------------------------------------------------------------------

#define ImGui_Print(...) ImGui::Text("%s", std::format(__VA_ARGS__).c_str())

constexpr auto ImGui_ToggleButtonSpacing = 4.f;
constexpr auto ImGui_TopWindowPaddingAdjustment = -3.f;

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

float FromSNorm(int16_t value)
{
    return std::clamp(float(value) / 32767.f, -1.f, 1.f);
}

// -----------------------------------------------------------------------------

bool gui = false;
std::vector<std::filesystem::path> initial_script_paths;

void ParseArgs(int argc, char* argv[])
{
    if (argc > 1 && std::string_view(argv[1]) == "--gui") {
        gui = true;
    }

    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);
        if (arg == "--gui") gui = true;
        else {
            auto path = std::filesystem::path(arg);
            path = std::filesystem::canonical(path);

            if (!std::filesystem::exists(path)) {
                Error("Error: could not find script file: {}", path.string());
            }

            initial_script_paths.emplace_back(path);
        }
    }
}

// -----------------------------------------------------------------------------

std::unordered_set<SDL_Joystick*> joysticks;

std::chrono::steady_clock::time_point last_script_run = {};
std::chrono::duration<double, std::nano> average_script_dur;
double average_script_util = 0.0;

struct Script
{
    std::filesystem::path path;
    std::optional<sol::state> lua;
    std::vector<sol::function> callbacks;
    std::vector<VirtualJoystick*> vjoysticks;

    bool disabled = true;
    std::string error;

    void Disable()
    {
        for (auto& joystick : vjoysticks) {
            joystick->Destroy();
        }
        vjoysticks.clear();
        callbacks.clear();
        lua = std::nullopt;

        disabled = true;
    }

    void Destroy()
    {
        Disable();
        delete this;
    }
};

std::vector<Script*> scripts;
std::vector<Script*> scripts_delete_queue;

void UnloadScript(const std::filesystem::path& script_path)
{
    Log("Unloading [{}]", script_path.string());
    std::erase_if(scripts, [&](auto& script) {
        Log(" - [{}]: {}", script->path.string(), script->path == script_path);
        if (script->path != script_path) return false;
        script->Destroy();
        return true;
    });
}

void QueueUnloadScript(Script* script)
{
    scripts_delete_queue.emplace_back(script);
}

void FlushScriptDeleteQueue()
{
    if (scripts_delete_queue.empty()) return;

    for (auto* script : scripts_delete_queue) {
        script->Destroy();
        std::erase(scripts, script);
    }
    scripts_delete_queue.clear();
}

void ReportScriptError(Script* script, const sol::error& error)
{
    Log("Error in callback: {}", error.what());
    Log("In script: {}", script->path.string());
    script->error = error.what();
}

void LoadScript(Script* script)
{
    script->Disable();

    auto& lua = script->lua.emplace();

    lua.open_libraries(sol::lib::base, sol::lib::math);

    struct LuaVirtualJoystick {
        VirtualJoystick* joystick;
    };

    lua.new_usertype<LuaVirtualJoystick>("VirtualJoystick",
        "SetAxis",   [](LuaVirtualJoystick& self, uint32_t i, float v) { self.joystick->SetAxis(i, v); },
        "SetButton", [](LuaVirtualJoystick& self, uint32_t i, bool v) { self.joystick->SetButton(i, v); });

    lua.set_function("CreateVirtualJoystick", [script](const sol::table& table) -> LuaVirtualJoystick {
        auto vjoy = CreateVirtualJoystick({
            .name        = table["name"].get<std::string>(),
            .device_id   = table["device_id"].get_or<uint8_t>(0),
            .version     = table["version"].get_or<uint16_t>(0),
            .vendor_id   = table["vendor_id"].get_or<uint16_t>(0),
            .product_id  = table["product_id"].get_or<uint16_t>(0),
            .num_axes    = table["num_axes"].get_or<uint16_t>(0),
            .num_buttons = table["num_buttons"].get_or<uint16_t>(0),
        });
        script->vjoysticks.emplace_back(vjoy);
        return {vjoy};
    });

    struct LuaJoystick {
        SDL_Joystick* joystick;
    };

    lua.new_usertype<LuaJoystick>("Joystick",
        "GetAxis",   [](LuaJoystick& self, uint32_t i) { return FromSNorm(SDL_GetJoystickAxis(self.joystick, i)); },
        "GetButton", [](LuaJoystick& self, uint32_t i) { return SDL_GetJoystickButton(self.joystick, i); });

    lua.set_function("FindJoystick", [&](uint16_t vendor_id, uint16_t product_id) -> std::optional<LuaJoystick> {
        for (auto joystick : joysticks) {
            if (vendor_id != SDL_GetJoystickVendor(joystick)) continue;
            if (product_id != SDL_GetJoystickProduct(joystick)) continue;

            return LuaJoystick{joystick};
        }

        return std::nullopt;
    });

    lua.set_function("Register", [script](sol::function f) {
        script->callbacks.emplace_back(std::move(f));
    });

    try {
        lua.script_file(script->path.string());
        script->disabled = false;
    } catch (const sol::error& e) {
        ReportScriptError(script, e);
        script->Disable();
    }
}

void LoadScript(const std::filesystem::path& script_path)
{
    UnloadScript(script_path);

    auto script = new Script{script_path};

    LoadScript(script);

    scripts.emplace_back(script);
}

// -----------------------------------------------------------------------------

SDL_Window* window = {};
SDL_GLContext context = {};

void Initialize()
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

    auto sdl_systems = SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;
    if (gui) {
        sdl_systems |= SDL_INIT_VIDEO;
    }

    SDL_Init(sdl_systems);
    SDL_SetJoystickEventsEnabled(true);
}

void OpenGUI()
{
    if (!gui) return;

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
}

// -----------------------------------------------------------------------------

uint64_t frame = 0;
bool joystick_event;

bool ProcessEvents()
{
    bool wait = frame++ > 1;

    joystick_event = false;

    SDL_Event event;
    while (wait ? SDL_WaitEvent(&event) : SDL_PollEvent(&event)) {
        wait = false;
        if (gui) {
            ImGui_ImplSDL3_ProcessEvent(&event);
        }
        switch (event.type) {
            case SDL_EVENT_QUIT:
                Log("Quitting");
                SDL_Quit();
                return false;

            case SDL_EVENT_JOYSTICK_ADDED:
                {
                    auto joystick = SDL_OpenJoystick(event.jdevice.which);
                    Log("Joystick added: {}", SDL_GetJoystickName(joystick));
                    joysticks.insert(joystick);
                    joystick_event = true;
                }
                break;

            case SDL_EVENT_JOYSTICK_REMOVED:
                {
                    auto joystick = SDL_GetJoystickFromID(event.jdevice.which);
                    Log("Joystick removed: {}", SDL_GetJoystickName(joystick));
                    joysticks.erase(joystick);
                    joystick_event = true;
                }
                break;

            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
            case SDL_EVENT_JOYSTICK_BALL_MOTION:
            case SDL_EVENT_JOYSTICK_HAT_MOTION:
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
            case SDL_EVENT_JOYSTICK_UPDATE_COMPLETE:
                joystick_event = true;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------

void UpdateJoysticks()
{
    if (joystick_event) {
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& script : scripts) {
            for (auto& callback : script->callbacks) {
                bool to_disable = false;
                std::optional<std::string> error;
                {
                    auto res = callback.call();
                    if (!res.valid()) {
                        ReportScriptError(script, res);
                        to_disable = true;
                    }
                }
                if (to_disable) {
                    script->Disable();
                    break;
                }
            }
        }
        FlushScriptDeleteQueue();
        auto end = std::chrono::high_resolution_clock::now();
        average_script_dur = average_script_dur * 0.95 + (end - start) * 0.05;

        auto run = std::chrono::steady_clock::now();
        auto diff = (run - last_script_run);
        last_script_run = run;
        auto util = average_script_dur / diff;
        average_script_util = average_script_util * 0.95 + util * 0.05;
    }

    for (auto& script : scripts) {
        for (auto* vjoy : script->vjoysticks) {
            vjoy->Update();
        }
    }
}

// -----------------------------------------------------------------------------

template<typename MenuFn>
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

void DrawStatsPanel()
{
    Defer _ = [] { ImGui::End(); };
    if (!ImGui::Begin("Stats")) return;

    ImGui_Print("Frame: {}", frame);
    ImGui_Print("Script Time: {} ({:.3f}%)", DurationToString(average_script_dur), average_script_util * 100.f);
}

void DrawGUI()
{
    if (!gui) return;

    BeginFrame([] {
        DrawFileMenu();
    });
    DrawLoadedScriptPanel();
    DrawVirtualJoysticksPanel();
    DrawJoystickInputViewer();
    DrawStatsPanel();
    EndFrame();
}

// -----------------------------------------------------------------------------

int main(int argc, char* argv[]) try
{
    ParseArgs(argc, argv);
    Initialize();
    OpenGUI();
    for (auto& script_path : initial_script_paths) {
        LoadScript(script_path);
    }

    while (ProcessEvents()) {
        DrawGUI();
        UpdateJoysticks();
    }
}
catch (const std::exception& e)
{
    Log("Exception: {}", e.what());
}
catch (...)
{
    Log("Uncaught Exception");
}
