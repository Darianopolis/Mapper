#include <vjoystick.hpp>

#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

#include <array>
#include <vector>
#include <ranges>

constexpr std::array axis_codes {
    ABS_X,
    ABS_Y,
    ABS_Z,
    ABS_RX,
    ABS_RY,
    ABS_RZ,

    ABS_THROTTLE,
    ABS_RUDDER,
    ABS_WHEEL,
    ABS_GAS,
    ABS_BRAKE,

    ABS_HAT0X,
    ABS_HAT0Y,
    ABS_HAT1X,
    ABS_HAT1Y,
    ABS_HAT2X,
    ABS_HAT2Y,
    ABS_HAT3X,
    ABS_HAT3Y,
};

constexpr int16_t axis_max_value = 32767;

constexpr std::array button_codes {
    BTN_TRIGGER,
    BTN_THUMB,
    BTN_THUMB2,
    BTN_TOP,
    BTN_TOP2,
    BTN_PINKIE,

    BTN_BASE,
    BTN_BASE2,
    BTN_BASE3,
    BTN_BASE4,
    BTN_BASE5,
    BTN_BASE6,

    BTN_0,
    BTN_1,
    BTN_2,
    BTN_3,
    BTN_4,
    BTN_5,
    BTN_6,
    BTN_7,
    BTN_8,
    BTN_9,
};

struct VirtualJoystick_EvDev : VirtualJoystick
{
    libevdev* dev = {};
    libevdev_uinput* uidev = {};

    std::array<float, max_axis_count> last_axes;
    std::array<bool, max_button_count> last_buttons;
};

VirtualJoystick* CreateVirtualJoystick(const VirtualJoystickDesc& desc)
{
    auto vjoy = new VirtualJoystick_EvDev{{desc}};
    vjoy->dev = libevdev_new();
    libevdev_set_name(vjoy->dev, desc.name.c_str());
    libevdev_set_id_bustype(vjoy->dev, BUS_VIRTUAL);
    libevdev_set_id_vendor(vjoy->dev, desc.vendor_id);
    libevdev_set_id_product(vjoy->dev, desc.product_id);
    libevdev_set_id_version(vjoy->dev, desc.version);

    if (desc.num_axes) {
        libevdev_enable_event_type(vjoy->dev, EV_ABS);
        for (uint32_t i = 0; i < vjoy->num_axes; ++i) {
            input_absinfo info {
                .minimum = -axis_max_value,
                .maximum =  axis_max_value,
                .resolution = 1,
            };
            libevdev_enable_event_code(vjoy->dev, EV_ABS, axis_codes[i], &info);
        }
    }

    if (desc.num_buttons) {
        libevdev_enable_event_type(vjoy->dev, EV_KEY);
        for (uint32_t i = 0; i < vjoy->num_buttons; ++i) {
            libevdev_enable_event_code(vjoy->dev, EV_KEY, button_codes[i], nullptr);
        }
    }

    libevdev_uinput_create_from_device(vjoy->dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &vjoy->uidev);

    return vjoy;
}

void VirtualJoystick::Destroy()
{
    auto self = static_cast<VirtualJoystick_EvDev*>(this);

    libevdev_uinput_destroy(self->uidev);
    libevdev_free(self->dev);

    delete self;
}

bool VirtualJoystick::Update()
{
    auto self = static_cast<VirtualJoystick_EvDev*>(this);

    bool any_change = false;

    constexpr auto update = [](auto& last, auto cur) -> bool {
        if (cur != last) {
            last = cur;
            return true;
        }
        return false;
    };

    for (uint32_t i = 0; i < self->num_axes; ++i) {
        if (!update(self->last_axes[i], self->axes[i])) continue;
        libevdev_uinput_write_event(self->uidev, EV_ABS, axis_codes[i], int16_t(self->axes[i] * axis_max_value));
        any_change = true;
    }

    for (uint32_t i = 0; i < self->num_buttons; ++i) {
        if (!update(self->last_buttons[i], self->buttons[i])) continue;
        libevdev_uinput_write_event(self->uidev, EV_KEY, button_codes[i], self->buttons[i]);
        any_change = true;
    }

    if (any_change) {
        libevdev_uinput_write_event(self->uidev, EV_SYN, SYN_REPORT, 0);
    }

    return any_change;
}
