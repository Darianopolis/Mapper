#include <mapper.hpp>

#include <sol/sol.hpp>

#include <SDL3/SDL.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#include <SDL3/SDL_opengl.h>
#pragma clang diagnostic pop

#include <SDL3/SDL_hints.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include <cmath>
#include <format>
#include <chrono>
#include <filesystem>
#include <unordered_set>

#include "vjoystick.hpp"

#define ImGui_Print(...) ImGui::Text("%s", std::format(__VA_ARGS__).c_str())

float from_snorm(int16_t value)
{
    return std::clamp(float(value) / 32767.f, -1.f, 1.f);
}

int main(int argc, char* argv[]) try
{
    mapper::Log("started");

    bool gui = false;
    std::filesystem::path script_path;

    if (argc > 1 && std::string_view(argv[1]) == "--gui") {
        gui = true;
    }

    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);
        if (arg == "-G" || arg == "--gui") gui = true;
        else {
            script_path = arg;

            if (!std::filesystem::exists(script_path)) {
                mapper::Error("Error: could not find script file: {}", script_path.string());
            }
        }
    }

    std::vector<VirtualJoystick*> vjoysticks;
    std::unordered_set<SDL_Joystick*> joysticks;

    std::chrono::steady_clock::time_point last_script_run = {};
    std::chrono::duration<double, std::nano> average_script_dur;
    double average_script_util = 0.0;

    sol::state lua;

    struct LuaVirtualJoystick {
        VirtualJoystick* vjoy;
    };

    lua.new_usertype<LuaVirtualJoystick>("VirtualJoystick",
        "SetAxis",   [](LuaVirtualJoystick& self, uint32_t i, float v) { self.vjoy->SetAxis(i, v); },
        "SetButton", [](LuaVirtualJoystick& self, uint32_t i, bool v) { self.vjoy->SetButton(i, v); },
        "Update",    [](LuaVirtualJoystick& self) { self.vjoy->Update(); });

    lua.set_function("CreateVirtualJoystick", [&](const sol::table& table) -> LuaVirtualJoystick {
        auto vjoy = CreateVirtualJoystick({
            .name = table["name"].get<std::string>(),
            .device_id = table["device_id"].get_or<uint8_t>(0),
            .version = table["version"].get_or<uint16_t>(0),
            .vendor_id = table["vendor_id"].get_or<uint16_t>(0),
            .product_id = table["product_id"].get_or<uint16_t>(0),
            .num_axes = table["num_axes"].get_or<uint16_t>(0),
            .num_buttons = table["num_axes"].get_or<uint16_t>(0),
        });
        vjoysticks.emplace_back(vjoy);
        return {vjoy};
    });

    struct LuaJoystick {
        SDL_Joystick* joystick;
    };

    lua.new_usertype<LuaJoystick>("Joystick",
        "GetAxis",   [](LuaJoystick& self, uint32_t i) { return from_snorm(SDL_GetJoystickAxis(self.joystick, i)); },
        "GetButton", [](LuaJoystick& self, uint32_t i) { return SDL_GetJoystickButton(self.joystick, i); });

    lua.set_function("FindJoystick", [&](uint16_t vendor_id, uint16_t product_id) -> std::optional<LuaJoystick> {
        for (auto joystick : joysticks) {
            if (vendor_id != SDL_GetJoystickVendor(joystick)) continue;
            if (product_id != SDL_GetJoystickProduct(joystick)) continue;

            return LuaJoystick{joystick};
        }

        return std::nullopt;
    });

    std::vector<sol::function> callbacks;

    lua.set_function("Register", [&](sol::function f) {
        callbacks.emplace_back(std::move(f));
    });

    mapper::Log("Initialized Lua state");

// -----------------------------------------------------------------------------

    if (!script_path.empty()) {
        lua.script_file(script_path.string());
    }

    mapper::Log("Loaded Lua script");

// -----------------------------------------------------------------------------

    // Init SDL

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

    auto sdl_systems = SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;
    if (gui) {
        sdl_systems |= SDL_INIT_VIDEO;
    }

    SDL_Init(sdl_systems);

    // Create Window

    SDL_Window* window = {};
    SDL_GLContext context = {};
    if (gui) {
        window = SDL_CreateWindow("Mapper", 1280, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

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

    // Main loop

    SDL_SetJoystickEventsEnabled(true);

    uint64_t frame = 0;

    for (;;) {

        // Process Events

        bool wait = frame++ > 1;

        SDL_Event event;
        while (wait ? SDL_WaitEvent(&event) : SDL_PollEvent(&event)) {
            wait = false;
            if (gui) {
                ImGui_ImplSDL3_ProcessEvent(&event);
            }
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    mapper::Log("Quitting");
                    SDL_Quit();
                    return EXIT_SUCCESS;

                case SDL_EVENT_JOYSTICK_ADDED:
                    {
                        auto joystick = SDL_OpenJoystick(event.jdevice.which);
                        mapper::Log("Joystick added: {}", SDL_GetJoystickName(joystick));
                        joysticks.insert(joystick);
                    }
                    break;

                case SDL_EVENT_JOYSTICK_REMOVED:
                    {
                        auto joystick = SDL_GetJoystickFromID(event.jdevice.which);
                        mapper::Log("Joystick removed: {}", SDL_GetJoystickName(joystick));
                        joysticks.erase(joystick);
                    }
                    break;
            }
        }

        // Start Frame

        if (gui) {
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
                | ImGuiWindowFlags_NoNavFocus;

            // Register dockspace
            bool show = true;
            ImGui::Begin("Dockspace", &show, dockspace_window_flags);
            ImGui::PopStyleVar(3);
            ImGui::DockSpace(ImGui::GetID("DockspaceID"), ImVec2(0.f, 0.f), dockspace_flags);

            ImGui::End();
        }

        // Process script callbacks

        {
            auto start = std::chrono::high_resolution_clock::now();
            for (auto& callback : callbacks) {
                auto res = callback.call();
                if (!res.valid()) {
                    sol::error err = res;
                    mapper::Log("Error in callback: {}", err.what());
                    callbacks.clear();
                    break;
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            average_script_dur = average_script_dur * 0.95 + (end - start) * 0.05;

            auto run = std::chrono::steady_clock::now();
            auto diff = (run - last_script_run);
            last_script_run = run;
            auto util = average_script_dur / diff;
            average_script_util = average_script_util * 0.95 + util * 0.05;
        }

        // Virtual joystick control panels

        if (gui) {
            for (auto* vjoy : vjoysticks) {
                if (ImGui::Begin(std::format("Virtual Device: {}", vjoy->name).c_str())) {
                    if (ImGui::Button("Reset")) {
                        for (uint32_t i = 0; i < vjoy->num_axes; ++i) {
                            vjoy->SetAxis(i, 0.f);
                        }
                        for (uint32_t i = 0; i < vjoy->num_buttons; ++i) {
                            vjoy->SetButton(i, false);
                        }
                    }

                    for (uint32_t i = 0; i < vjoy->num_axes; ++i) {
                        float v = vjoy->GetAxis(i);
                        if (ImGui::SliderFloat(std::format("{}##{}.axis", i, vjoy->name).c_str(), &v, -1.f, 1.f)) {
                            vjoy->SetAxis(i, v);
                        }
                    }

                    for (uint32_t i = 0; i < vjoy->num_buttons; ++i) {
                        bool pressed = vjoy->GetButton(i);
                        if (ImGui::Checkbox(std::format("{}##{}.button", i, vjoy->name).c_str(), &pressed)) {
                            vjoy->SetButton(i, pressed);
                        }
                    }
                }

                ImGui::End();
            }
        }

        // Update virtual joysticks

        for (auto* vjoy : vjoysticks) {
            vjoy->Update();
        }

// -----------------------------------------------------------------------------

        if (!gui) continue;

        // Draw Contents

        if (ImGui::Begin("Joystick Input Viewer")) {

            for (auto joystick : joysticks) {
                auto name = SDL_GetJoystickName(joystick);
                auto vendor_id = SDL_GetJoystickVendor(joystick);
                auto product_id = SDL_GetJoystickProduct(joystick);
                auto version = SDL_GetJoystickProductVersion(joystick);

                auto guid = SDL_GetJoystickGUID(joystick);
                /* SDL_GUID : (bus_type, 0, vendor_id, 0, product_id, 0, version, 0) */
                uint16_t bus_type;
                std::memcpy(&bus_type, guid.data, 2);

                if (!ImGui::CollapsingHeader(std::format("({:#06x}/{:#06x}/{:#06x}/{:#06x}) {}##{}", bus_type, vendor_id, product_id, version, name, (void*)joystick).c_str())) continue;

                ImGui_Print("Path: {}", SDL_GetJoystickPath(joystick));

                for (int i = 0; i < SDL_GetNumJoystickAxes(joystick); ++i) {
                    auto raw = SDL_GetJoystickAxis(joystick, i);

                    auto normalized = from_snorm(raw);
                    ImGui::SliderFloat(std::format("{}###axis.{}.{}", raw, i, (void*)joystick).c_str(), &normalized, -1.f, 1.f, "%.3f", ImGuiSliderFlags_NoInput);
                }

                for (int i = 0; i < SDL_GetNumJoystickButtons(joystick); ++i) {
                    auto pressed = SDL_GetJoystickButton(joystick, i);

                    if (i > 0) ImGui::SameLine();
                    ImGui::Checkbox(std::format("###button.{}.{}", i, (void*)joystick).c_str(), &pressed);
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
        ImGui::End();

        if (ImGui::Begin("Stats")) {
            ImGui_Print("Frame: {}", frame);
            ImGui_Print("Script Time: {} ({:.3f}%)", mapper::DurationToString(average_script_dur), average_script_util * 100.f);
        }
        ImGui::End();

        // Render

        ImGui::Render();
        int w, h;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.2, 0.2, 0.2, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
}
catch (const std::exception& e)
{
    mapper::Log("Exception: {}", e.what());
}
catch (...)
{
    mapper::Log("Uncaught Exception");
}
