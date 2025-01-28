#include <glad/gl.h>

#include <sol/sol.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_joystick.h>

#include <SDL3/SDL_hints.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include <cmath>
#include <print>
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

int16_t to_snorm16(float value)
{
    return int16_t(std::clamp(value, -1.f, 1.f) * 32767);
}

int main(int argc, char* argv[]) try
{
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
                std::println("Error: could not find script file: {}", script_path.c_str());
                return EXIT_FAILURE;
            }
        }
    }

    std::vector<VirtualJoystick*> vjoysticks;
    std::unordered_set<SDL_JoystickID> joysticks;
    std::unordered_set<SDL_JoystickID> gamepads;

    sol::state lua;
    {
        int x = 0;
        lua.set_function("beep", [&x]{ ++x; });
        lua.script("beep()");
        std::println("x = {}", x);
    }

    struct LuaVirtualJoystick {
        VirtualJoystick* vjoy;
    };

    lua.open_libraries(sol::lib::base, sol::lib::math);

    lua.new_usertype<LuaVirtualJoystick>("VirtualJoystick",
        "SetAxis",   [](LuaVirtualJoystick& self, uint32_t i, float v) { self.vjoy->SetAxis(i, v); },
        "SetButton", [](LuaVirtualJoystick& self, uint32_t i, bool v) { self.vjoy->SetButton(i, v); },
        "Update",    [](LuaVirtualJoystick& self) { self.vjoy->Update(); });

    lua.set_function("CreateVirtualJoystick", [&](const sol::table& table) -> LuaVirtualJoystick {
        std::println("Creating virtual joystick");
        auto vjoy = CreateVirtualJoystick({
            .name = table["name"].get<std::string>(),
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
        "GetButton", [](LuaJoystick& self, uint32_t i) { return SDL_GetJoystickButton(self.joystick, i); },
        "SetAxis",   [](LuaJoystick& self, uint32_t i, float v) { SDL_SetJoystickVirtualAxis(self.joystick, i, to_snorm16(v)); },
        "SetButton", [](LuaJoystick& self, uint32_t i, bool v) { SDL_SetJoystickVirtualButton(self.joystick, i, v); });

    lua.set_function("FindJoystick", [&](uint16_t vendor_id, uint16_t product_id) -> std::optional<LuaJoystick> {
        for (auto id : joysticks) {
            auto joystick = SDL_GetJoystickFromID(id);

            if (!joystick) {
                if (!(joystick = SDL_OpenJoystick(id))) {
                    continue;
                }
            }

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

// -----------------------------------------------------------------------------

    if (!script_path.empty()) {
        lua.script_file(script_path);
    }

// -----------------------------------------------------------------------------

    // Init SDL

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    auto sdl_systems = SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;
    if (gui) sdl_systems |= SDL_INIT_VIDEO;

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

        // Load OpenGL functions

        auto version = gladLoadGL(SDL_GL_GetProcAddress);
        if (!version) {
            std::println("Failed to load GL functions!\n");
            SDL_Quit();
            return EXIT_FAILURE;
        } else {
            std::println("Loaded GL {}.{}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
        }

        // Init ImGui

        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui_ImplSDL3_InitForOpenGL(window, context);
        ImGui_ImplOpenGL3_Init("#version 460");
    }

    // Main loop

    uint64_t frame = 0;

    SDL_Event event;
    for (;;) {

        // Poll Events

        bool wait = frame > 0;

        while (wait ? SDL_WaitEvent(&event) : SDL_PollEvent(&event)) {
            if (gui) {
                wait = false;
                ImGui_ImplSDL3_ProcessEvent(&event);
            }
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    std::println("Quitting");
                    SDL_Quit();
                    return EXIT_SUCCESS;

                case SDL_EVENT_JOYSTICK_ADDED:
                    joysticks.insert(event.jdevice.which);
                    break;

                case SDL_EVENT_JOYSTICK_REMOVED:
                    joysticks.erase(event.jdevice.which);
                    break;

                case SDL_EVENT_GAMEPAD_ADDED:
                    gamepads.insert(event.gdevice.which);
                    break;

                case SDL_EVENT_GAMEPAD_REMOVED:
                    gamepads.erase(event.gdevice.which);
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

        // Forward Taranis Events on to virtual joystick

        {
            if (gui) {
                for (auto* vdevice : vjoysticks) {
                    if (ImGui::Begin(std::format("Virtual Device: {}", vdevice->name).c_str())) {
                        if (ImGui::Button("Reset")) {
                            for (uint32_t i = 0; i < vdevice->num_axes; ++i) {
                                vdevice->SetAxis(i, 0.f);
                            }
                            for (uint32_t i = 0; i < vdevice->num_buttons; ++i) {
                                vdevice->SetButton(i, false);
                            }
                        }

                        for (uint32_t i = 0; i < vdevice->num_axes; ++i) {
                            float v = vdevice->GetAxis(i);
                            if (ImGui::SliderFloat(std::format("{}##{}.axis", i, vdevice->name).c_str(), &v, -1.f, 1.f)) {
                                vdevice->SetAxis(i, v);
                            }
                        }

                        for (uint32_t i = 0; i < vdevice->num_buttons; ++i) {
                            bool pressed = vdevice->GetButton(i);
                            if (ImGui::Checkbox(std::format("{}##{}.button", i, vdevice->name).c_str(), &pressed)) {
                                vdevice->SetButton(i, pressed);
                            }
                        }

                        vdevice->Update();
                    }

                    ImGui::End();
                }
            }

            for (auto& callback : callbacks) {
                auto res = callback.call();
                if (!res.valid()) {
                    sol::error err = res;
                    std::println("Error in callback: {}", err.what());
                    callbacks.clear();
                }
            }
        }

        if (!gui) continue;

        // Draw Contents

        {
            if (ImGui::Begin("Joystick Input Viewer")) {

                for (auto id : joysticks) {
                    auto joystick = SDL_GetJoystickFromID(id);

                    if (!joystick) {
                        if (!(joystick = SDL_OpenJoystick(id))) {
                            continue;
                        }
                    }

                    auto name = SDL_GetJoystickName(joystick);
                    auto vendor_id = SDL_GetJoystickVendor(joystick);
                    auto product_id = SDL_GetJoystickProduct(joystick);
                    auto version = SDL_GetJoystickProductVersion(joystick);

                    auto guid = SDL_GetJoystickGUID(joystick);
                    /* SDL_GUID : (bus_type, 0, vendor_id, 0, product_id, 0, version, 0) */
                    uint16_t bus_type;
                    std::memcpy(&bus_type, guid.data, 2);

                    if (!ImGui::CollapsingHeader(std::format("({:#06x}/{:#06x}/{:#06x}/{:#06x}) {}", bus_type, vendor_id, product_id, version, name).c_str())) continue;

                    for (int i = 0; i < SDL_GetNumJoystickAxes(joystick); ++i) {
                        auto axis = SDL_GetJoystickAxis(joystick, i);

                        auto normalized = from_snorm(axis);
                        ImGui::SliderFloat(std::format("##axis.{}.{}", i, id).c_str(), &normalized, -1.f, 1.f, "%.3f", ImGuiSliderFlags_NoInput);
                    }

                    for (int i = 0; i < SDL_GetNumJoystickButtons(joystick); ++i) {
                        auto pressed = SDL_GetJoystickButton(joystick, i);

                        if (i > 0) ImGui::SameLine();
                        ImGui::Checkbox(std::format("##button.{}.{}", i, id).c_str(), &pressed);
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

            if (ImGui::Begin("Gamepad Input Viewer")) {
                for (auto id : gamepads) {
                    auto gamepad = SDL_GetGamepadFromID(id);

                    if (!gamepad) {
                        if (!(gamepad = SDL_OpenGamepad(id))) {
                            continue;
                        }
                    }

                    auto name = SDL_GetGamepadName(gamepad);

                    if (!ImGui::CollapsingHeader(name)) continue;

                    ImGui_Print("Axis.LStick.X = {}", SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX));
                    ImGui_Print("Axis.LStick.Y = {}", SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
                    ImGui_Print("Axis.RStick.X = {}", SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX));
                    ImGui_Print("Axis.RStick.Y = {}", SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY));
                    ImGui_Print("Axis.LTrigger = {}", SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
                    ImGui_Print("Axis.RTrigger = {}", SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));

                    ImGui_Print("Button.South = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH));
                    ImGui_Print("Button.East = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST));
                    ImGui_Print("Button.West = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST));
                    ImGui_Print("Button.North = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH));
                    ImGui_Print("Button.Back = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK));
                    ImGui_Print("Button.Guide = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_GUIDE));
                    ImGui_Print("Button.Start = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START));
                    ImGui_Print("Button.LStick = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK));
                    ImGui_Print("Button.RStick = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK));
                    ImGui_Print("Button.LShoulder = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
                    ImGui_Print("Button.RShoulder = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));
                    ImGui_Print("Button.DPad.Up = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP));
                    ImGui_Print("Button.DPad.Down = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN));
                    ImGui_Print("Button.DPad.Left = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT));
                    ImGui_Print("Button.DPad.Right = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT));
                    ImGui_Print("Button.Misc1 = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_MISC1));
                    ImGui_Print("Button.RPaddle1 = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1));
                    ImGui_Print("Button.LPaddle1 = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1));
                    ImGui_Print("Button.RPaddle2 = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2));
                    ImGui_Print("Button.LPaddle2 = {}", SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2));
                }
            }
            ImGui::End();
        }

        if (ImGui::Begin("Stats")) {
            ImGui_Print("Frame: {}", ++frame);
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
    std::println("Exception: {}", e.what());
}
catch (...)
{
    std::println("Uncaught Exception");
}
