#include "mapper.hpp"

void Script::Disable()
{
    for (auto& joystick : vjoysticks) {
        joystick->Destroy();
    }
    vjoysticks.clear();
    callbacks.clear();
    lua = std::nullopt;

    disabled = true;
}

void Script::Destroy()
{
    Disable();
    delete this;
}

void QueueUnloadScript(Script* script)
{
    scripts_delete_queue.emplace_back(script);
}

void FlushScriptDeleteQueue(SharedLockGuard& prior_lock)
{
    if (scripts_delete_queue.empty()) return;

    SharedLockGuard _{ prior_lock, LockState::Unique };

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

    lua.set_function("GetTime", []() -> double {
        return SDL_GetTicks() / 1000.0;
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

static
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

void LoadScript(const std::filesystem::path& script_path)
{
    UnloadScript(script_path);

    auto script = new Script{script_path};

    LoadScript(script);

    scripts.emplace_back(script);
}
