#include "device.hpp"

#include <imgui.h>

#include <print>

VirtualDevice::VirtualDevice()
{
    dev = libevdev_new();
    libevdev_set_name(dev, "Virtual Joystick");
    libevdev_set_id_vendor(dev, 0x0);
    libevdev_set_id_product(dev, 0x0);
    libevdev_enable_event_type(dev, EV_KEY);
    for (auto k : buttons) {
        libevdev_enable_event_code(dev, EV_KEY, k.mapping, nullptr);
    }
    libevdev_enable_event_type(dev, EV_ABS);
    for (auto a : axis) {
        input_absinfo info = {};
        info.minimum = -32767;
        info.maximum = 32767;
        info.resolution = 1;
        libevdev_enable_event_code(dev, EV_ABS, a.mapping, &info);
    }

    auto err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    if (err) {
        std::println("Error creating virtual device!");
    }
}

VirtualDevice::~VirtualDevice()
{
    libevdev_uinput_destroy(uidev);
}

void VirtualDevice::SendAxis(int index, float value)
{
    auto& a = axis[index];
    value = std::clamp(value, -1.f, 1.f);
    if (a.value == value) return;
    // std::println("Sending value: {}", value);
    a.value = value;
    int16_t snorm = int16_t(value * 32767);
    libevdev_uinput_write_event(uidev, EV_ABS, a.mapping, snorm);
    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
}

void VirtualDevice::SendButton(int index, bool pressed)
{
    auto& k = buttons[index];
    if (k.pressed == pressed) return;
    k.pressed = pressed;
    libevdev_uinput_write_event(uidev, EV_KEY, k.mapping, pressed);
    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
}

void VirtualDevice::DeviceGUI()
{
    if (!ImGui::Begin("Virtual Device")) return;

    for (uint32_t i = 0; i < axis.size(); ++i) {
        auto& a = axis[i];
        float v = a.value;
        if (ImGui::SliderFloat(std::format("Axis[{}]", a.name).c_str(), &v, -1.f, 1.f)) {
            SendAxis(i, v);
        }
    }

    for (uint32_t i = 0; i < buttons.size(); ++i) {
        auto& b = buttons[i];
        bool pressed = b.pressed;
        if (ImGui::Checkbox(std::format("Button[{}]", b.name).c_str(), &pressed)) {
            SendButton(i, pressed);
        }
    }

    ImGui::End();
}