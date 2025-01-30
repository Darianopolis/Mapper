#include <string>

#include <array>
#include <cstdint>
#include <algorithm>

constexpr uint32_t max_axis_count = 19;
constexpr uint32_t max_button_count = 128;

struct VirtualJoystickDesc
{
    std::string name = {};

    // Only used for vJoy backend
    uint8_t device_id = 0;

    // Only used for evdev backend
    uint16_t version = 0;
    uint16_t vendor_id = 0;
    uint16_t product_id = 0;

    uint16_t num_axes = 0;
    uint16_t num_buttons = 0;
};

struct VirtualJoystick : VirtualJoystickDesc
{
    std::array<float, max_axis_count> axes;
    std::array<bool, max_button_count> buttons;

    void Destroy();

    float  GetAxis(uint32_t index) { return    axes[index]; }
    bool GetButton(uint32_t index) { return buttons[index]; }

    void   SetAxis(uint32_t index, float value) {    axes[index] = std::clamp(value, -1.f, 1.f); }
    void SetButton(uint32_t index, bool  state) { buttons[index] = state;                        }

    void Update();
};

VirtualJoystick* CreateVirtualJoystick(const VirtualJoystickDesc& desc);
