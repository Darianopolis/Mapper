#include <vjoystick.hpp>

#include <common.hpp>

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <ranges>

#include "vjoy.hpp"

struct VirtualJoystick_VJoy : VirtualJoystick
{
    std::array<float, max_axis_count> last_axes;
    std::array<bool, max_button_count> last_buttons;
};

VirtualJoystick* CreateVirtualJoystick(const VirtualJoystickDesc& desc)
{
    if (!vjoy::Load()) {
        Error("[vJoy] Failed to load vJoy functions");
    }

    auto joy =  new VirtualJoystick_VJoy{{desc}};

    if (!vjoy::api::vJoyEnabled()) {
        Error("[vjoy] vJoy not detected!");
    }

    if (!vjoy::api::isVJDExists(joy->device_id)) {
        Error("[vJoy] No vJoy device with ID {} exists!", joy->device_id);
    }

    if (!vjoy::api::AcquireVJD(joy->device_id)) {
        auto owner_pid = vjoy::api::GetOwnerPid(joy->device_id);
        Error("[vJoy] Failed to acquire device with ID {}. Current owner PID = {}", joy->device_id, owner_pid);
    }

    if (auto avail_buttons = vjoy::api::GetVJDButtonNumber(joy->device_id); avail_buttons < desc.num_buttons) {
        Error("[vJoy] Device does not have enough buttons. Expected {} got {}", desc.num_buttons, avail_buttons);
    }

    return joy;
}

void VirtualJoystick::Destroy()
{
    auto self = static_cast<VirtualJoystick_VJoy*>(this);

    vjoy::api::RelinquishVJD(self->device_id);

    delete self;
}

bool VirtualJoystick::Update()
{
    auto self = static_cast<VirtualJoystick_VJoy*>(this);

    vjoy::api::JOYSTICK_POSITION p = {};
    p.bDevice = self->device_id;

    bool any_changed = false;

    auto axes_pm = std::array {
        &vjoy::api::JOYSTICK_POSITION::wAxisX,
        &vjoy::api::JOYSTICK_POSITION::wAxisY,
        &vjoy::api::JOYSTICK_POSITION::wAxisZ,
        &vjoy::api::JOYSTICK_POSITION::wAxisXRot,
        &vjoy::api::JOYSTICK_POSITION::wAxisYRot,
        &vjoy::api::JOYSTICK_POSITION::wAxisZRot,
        &vjoy::api::JOYSTICK_POSITION::wSlider,
        &vjoy::api::JOYSTICK_POSITION::wDial,
    };

    constexpr auto update = [](auto& last, auto cur) -> bool {
        if (cur != last) {
            last = cur;
            return true;
        }
        return false;
    };

    for (uint32_t i = 0; i < self->num_axes; ++i) {
        any_changed |= update(self->last_axes[i], self->axes[i]);

        p.*axes_pm[i] = 16'384 + (self->axes[i] * 16'383);
    }

    auto buttons_pm = std::array {
        &vjoy::api::JOYSTICK_POSITION::lButtons,
        &vjoy::api::JOYSTICK_POSITION::lButtonsEx1,
        &vjoy::api::JOYSTICK_POSITION::lButtonsEx2,
        &vjoy::api::JOYSTICK_POSITION::lButtonsEx3,
    };

    for (uint32_t i = 0; i < self->num_buttons; ++i) {
        any_changed |= update(self->last_buttons[i], self->buttons[i]);
        auto& dword = p.*buttons_pm[i / 32];
        auto mask = 1 << (i % 32);
        dword = (dword & ~mask) | (self->buttons[i] ? mask : 0);
    }

    if (any_changed) {
        if (!vjoy::api::UpdateVJD(p.bDevice, &p)) {
            Error("Failed to feed vJoy device: {}", p.bDevice);
        }
    }

    return any_changed;
}
