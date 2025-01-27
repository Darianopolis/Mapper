#pragma once

#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

#include <array>
#include <vector>

struct Axis
{
    int mapping;
    float value;
    const char* name;
};

struct Button
{
    int mapping;
    bool pressed;
    const char* name;
};

struct VirtualDevice
{
    libevdev* dev = {};
    libevdev_uinput* uidev = {};

    std::array<Axis, 4> axis_mappings = {
        Axis{ABS_X, 0.f, "Handbrake"},
        // Axis{ABS_Y, 0.f, "Throttle"},
        Axis{ABS_THROTTLE, 0.f, "Throttle"},
        // Axis{ABS_Z, 0.f, "Wheel"},
        Axis{ABS_WHEEL, 0.f, "Wheel"},
        // Axis{ABS_RX, 0.f, "Brake"},
        Axis{ABS_BRAKE, 0.f, "Brake"},
    };

    std::array<Button, 4> button_mappings = {
        Button{BTN_0, false, "Forward"},
        Button{BTN_1, false, "Back"},
        Button{BTN_2, false, "Shoulder"},
        Button{BTN_3, false, "LeftStick.Right"},
    };

    ~VirtualDevice();

    void Create();

    void SendAxis(int index, float value);
    void SendButton(int index, bool pressed);

    void DeviceGUI();
};
