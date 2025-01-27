#include "device.hpp"

#include <imgui.h>

#include <print>
#include <ranges>

// constexpr std::array axis_codes {
//     ABS_X,
//     ABS_Y,
//     ABS_Z,
//     ABS_RX,
//     ABS_RY,
//     ABS_RZ,
//     ABS_THROTTLE,
//     ABS_RUDDER,
//     ABS_WHEEL,
//     ABS_GAS,
//     ABS_BRAKE,
//     ABS_HAT0X,
//     ABS_HAT0Y,
//     ABS_HAT1X,
//     ABS_HAT1Y,
//     ABS_HAT2X,
//     ABS_HAT2Y,
//     ABS_HAT3X,
//     ABS_HAT3Y,
// };

// constexpr std::array button_codes {
//     BTN_0,
//     BTN_1,
//     BTN_2,
//     BTN_3,
//     BTN_4,
//     BTN_5,
//     BTN_6,
//     BTN_7,
//     BTN_8,
//     BTN_9,
//     BTN_MOUSE,
//     BTN_LEFT,
//     BTN_RIGHT,
//     BTN_MIDDLE,
//     BTN_SIDE,
//     BTN_EXTRA,
//     BTN_FORWARD,
//     BTN_BACK,
//     BTN_TASK,
//     BTN_JOYSTICK,
//     BTN_TRIGGER,
//     BTN_THUMB,
//     BTN_THUMB2,
//     BTN_TOP,
//     BTN_TOP2,
//     BTN_PINKIE,
//     BTN_BASE,
//     BTN_BASE2,
//     BTN_BASE3,
//     BTN_BASE4,
//     BTN_BASE5,
//     BTN_BASE6,
// };

void VirtualDevice::Create()
{
    dev = libevdev_new();
    libevdev_set_name(dev, "Virtual Joystick");
    libevdev_set_id_bustype(dev, BUS_VIRTUAL);
    libevdev_set_id_vendor(dev, 0);
    libevdev_set_id_product(dev, 0);
    libevdev_set_id_version(dev, 1);
    libevdev_enable_event_type(dev, EV_KEY);
    for (auto[i, button] : button_mappings | std::views::enumerate) {
        libevdev_enable_event_code(dev, EV_KEY, button.mapping, nullptr);
    }
    libevdev_enable_event_type(dev, EV_ABS);
    for (auto[i, axis] : axis_mappings | std::views::enumerate) {
        input_absinfo info = {};
        info.minimum = -32767;
        info.maximum = 32767;
        info.resolution = 1;
        libevdev_enable_event_code(dev, EV_ABS, axis.mapping, &info);
    }

    auto err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    if (err) {
        std::println("Error creating virtual device!");
    }
}

VirtualDevice::~VirtualDevice()
{
    libevdev_uinput_destroy(uidev);
    libevdev_free(dev);
}

void VirtualDevice::SendAxis(int index, float value)
{
    auto& axis = axis_mappings[index];
    value = std::clamp(value, -1.f, 1.f);
    if (axis.value == value) return;
    axis.value = value;
    int16_t snorm = int16_t(value * 32767);
    libevdev_uinput_write_event(uidev, EV_ABS, axis.mapping, snorm);
    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
}

void VirtualDevice::SendButton(int index, bool pressed)
{
    auto& button = button_mappings[index];
    if (button.pressed == pressed) return;
    button.pressed = pressed;
    libevdev_uinput_write_event(uidev, EV_KEY, button.mapping, pressed);
    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
}

void VirtualDevice::DeviceGUI()
{
    if (ImGui::Begin("Virtual Device")) {

        for (uint32_t i = 0; i < axis_mappings.size(); ++i) {
            auto& a = axis_mappings[i];
            float v = a.value;
            if (ImGui::SliderFloat(std::format("Axis[{}]", a.name).c_str(), &v, -1.f, 1.f)) {
                SendAxis(i, v);
            }
        }

        for (uint32_t i = 0; i < button_mappings.size(); ++i) {
            auto& b = button_mappings[i];
            bool pressed = b.pressed;
            if (ImGui::Checkbox(std::format("Button[{}]", b.name).c_str(), &pressed)) {
                SendButton(i, pressed);
            }
        }
    }

    ImGui::End();
}