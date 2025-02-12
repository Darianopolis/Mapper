#pragma once

#include "common.hpp"
#include "lock.hpp"
#include "vjoystick.hpp"

#include <algorithm>
#include <unordered_set>
#include <cstdint>
#include <optional>
#include <vector>
#include <cmath>
#include <format>
#include <chrono>
#include <filesystem>
#include <shared_mutex>

#include <SDL3/SDL.h>

#include <sol/sol.hpp>

inline
float FromSNorm(int16_t value)
{
    return std::clamp(float(value) / 32767.f, -1.f, 1.f);
}

// -----------------------------------------------------------------------------
//          Engine
// -----------------------------------------------------------------------------

inline uint64_t frame = 0;
inline std::unordered_set<SDL_Joystick*> joysticks;
inline std::shared_mutex engine_mutex;

void Initialize();
bool ProcessEvents();
void UpdateJoysticks();
void PushJoystickUpdateEvent();

// -----------------------------------------------------------------------------
//          Scripts
// -----------------------------------------------------------------------------

inline std::chrono::steady_clock::time_point last_script_run = {};
inline std::chrono::duration<double, std::nano> average_script_dur;
inline double average_script_util = 0.0;

struct Script
{
    std::filesystem::path path;
    std::optional<sol::state> lua;
    std::vector<sol::function> callbacks;
    std::vector<VirtualJoystick*> vjoysticks;

    bool disabled = true;
    std::string error;

    void Disable();
    void Destroy();
};

inline std::vector<Script*> scripts;
inline std::vector<Script*> scripts_delete_queue;

void QueueUnloadScript(Script* script);
void FlushScriptDeleteQueue(SharedLockGuard&);
void ReportScriptError(Script* script, const sol::error& error);
void LoadScript(Script* script);
void LoadScript(const std::filesystem::path& script_path);

// -----------------------------------------------------------------------------
//          GUI
// -----------------------------------------------------------------------------

inline uint64_t gui_frame = 0;

void OpenGUI();
void PushGUIRedrawEvent();
void CloseGUI();
