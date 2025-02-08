#include "mapper.hpp"

bool joystick_event;
Uint32 joystick_update_event;

void Initialize()
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD);
    SDL_SetJoystickEventsEnabled(true);

    joystick_update_event = SDL_RegisterEvents(1);
}

bool ProcessEvents()
{

    bool wait = frame++ > 1;

    joystick_event = false;

    SDL_Event event;
    while (wait ? SDL_WaitEvent(&event) : SDL_PollEvent(&event)) {
        wait = false;
        switch (event.type) {
            case SDL_EVENT_QUIT:
                Log("Quitting");
                SDL_Quit();
                return false;

            case SDL_EVENT_JOYSTICK_ADDED:
                {
                    std::scoped_lock _{ engine_mutex };
                    auto joystick = SDL_OpenJoystick(event.jdevice.which);
                    Log("Joystick added: {}", SDL_GetJoystickName(joystick));
                    joysticks.insert(joystick);
                    joystick_event = true;
                }
                break;

            case SDL_EVENT_JOYSTICK_REMOVED:
                {
                    std::scoped_lock _{ engine_mutex };
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
            default:
                if (event.type == joystick_update_event) {
                    joystick_event = true;
                }
        }
    }

    return true;
}

// -----------------------------------------------------------------------------

void UpdateJoysticks()
{
    std::scoped_lock _{ engine_mutex };

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
                    QueueUnloadScript(script);
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

    PushGUIRedrawEvent();
}

void PushJoystickUpdateEvent()
{
    SDL_Event event;
    event.type = joystick_update_event;
    SDL_PushEvent(&event);
}
