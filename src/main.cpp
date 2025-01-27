#include <glad/gl.h>

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
#include <unordered_set>

#include "device.hpp"

#define ImGui_Print(...) ImGui::Text("%s", std::format(__VA_ARGS__).c_str())

float from_snorm(int16_t value)
{
    return std::clamp(float(value) / 32767.f, -1.f, 1.f);
}

int main()
{
    VirtualDevice vdevice;

    // Init SDL

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    SDL_Init(SDL_INIT_VIDEO
        | SDL_INIT_JOYSTICK
        | SDL_INIT_GAMEPAD
        );

    // Create Window

    auto window = SDL_CreateWindow("Mapper", 1280, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    // Init OpenGL

    auto context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

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

    // Main loop

    std::unordered_set<SDL_JoystickID> joysticks;
    std::unordered_set<SDL_JoystickID> gamepads;

    SDL_Event event;
    for (;;) {

        // Poll Events

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
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

        // Forward Taranis Events on to virtual joystick

        {
            for (auto id : joysticks) {
                auto joystick = SDL_GetJoystickFromID(id);
                if (!joystick && !(joystick = SDL_OpenJoystick(id))) continue;

                auto product = SDL_GetJoystickProduct(joystick);
                auto vendor = SDL_GetJoystickVendor(joystick);

                // std::println("{:#x}/{:#x}", vendor, product);

                if (vendor != 0x0483 || product != 0x5710) continue;

                auto throttle = from_snorm(SDL_GetJoystickAxis(joystick, 0));
                auto wheel_in = from_snorm(SDL_GetJoystickAxis(joystick, 1));
                auto brake_handbrake = from_snorm(SDL_GetJoystickAxis(joystick, 3));
                auto brake = std::clamp(brake_handbrake, 0.f, 1.f);
                auto handbrake = std::clamp(brake_handbrake, -1.f, 0.f);

                wheel_in = std::clamp(wheel_in * 1.1f, -1.f, 1.f);
                // auto wheel_out = std::copysignf(std::pow(std::abs(wheel_in), 2.5f), wheel_in);
                auto wheel_out = wheel_in;

                vdevice.SendAxis(0, handbrake);
                vdevice.SendAxis(1, throttle);
                vdevice.SendAxis(2, wheel_out);
                vdevice.SendAxis(3, brake);

                auto lean = from_snorm(SDL_GetJoystickAxis(joystick, 2));
                auto shoulder = from_snorm(SDL_GetJoystickAxis(joystick, 4));

                vdevice.SendButton(0, lean >  0.25f);
                vdevice.SendButton(1, lean < -0.25f);
                vdevice.SendButton(2, shoulder > 0.f);
                vdevice.SendButton(3, handbrake < -0.35f);
            }
        }

        // Start Frame

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Dockspace

        {
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

                    auto type = SDL_GetJoystickType(joystick);
                    const char* type_str;
                    switch (type) {
                        case SDL_JOYSTICK_TYPE_UNKNOWN: type_str = "UNKNOWN"; break;
                        case SDL_JOYSTICK_TYPE_GAMEPAD: type_str = "GAMEPAD"; break;
                        case SDL_JOYSTICK_TYPE_WHEEL: type_str = "WHEEL"; break;
                        case SDL_JOYSTICK_TYPE_ARCADE_STICK: type_str = "ARCADE_STICK"; break;
                        case SDL_JOYSTICK_TYPE_FLIGHT_STICK: type_str = "FLIGHT_STICK"; break;
                        case SDL_JOYSTICK_TYPE_DANCE_PAD: type_str = "DANCE_PAD"; break;
                        case SDL_JOYSTICK_TYPE_GUITAR: type_str = "GUITAR"; break;
                        case SDL_JOYSTICK_TYPE_DRUM_KIT: type_str = "DRUM_KIT"; break;
                        case SDL_JOYSTICK_TYPE_ARCADE_PAD: type_str = "ARCADE_PAD"; break;
                        case SDL_JOYSTICK_TYPE_THROTTLE: type_str = "THROTTLE"; break;
                        default:
                            ;
                    }

                    if (!ImGui::CollapsingHeader(std::format("{} ({})", name, type_str).c_str())) continue;

                    for (int i = 0; i < SDL_GetNumJoystickAxes(joystick); ++i) {
                        auto axis = SDL_GetJoystickAxis(joystick, i);

                        // ImGui_Print("Axis[{}] = {}", i, axis);
                        auto normalized = from_snorm(axis);
                        ImGui::SliderFloat(std::format("##axis.{}.{}", i, id).c_str(), &normalized, -1.f, 1.f, "%.3f", ImGuiSliderFlags_NoInput);
                    }

                    for (int i = 0; i < SDL_GetNumJoystickButtons(joystick); ++i) {
                        auto pressed = SDL_GetJoystickButton(joystick, i);

                        // ImGui_Print("Button[{}] = {}", i, pressed);

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

                ImGui::End();
            }

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

                ImGui::End();
            }

            vdevice.DeviceGUI();
        }

        // ImGui::ShowDemoWindow();

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
