
#include <SDL3/SDL_joystick.h>

#include <string>

struct VirtualJoystickDesc
{
    std::string name = {};
    uint16_t version = 0;
    uint16_t vendor_id = 0;
    uint16_t product_id = 0;

    uint16_t num_axes = 0;
    uint16_t num_buttons = 0;
};

struct VirtualJoystick : VirtualJoystickDesc {
    void Destroy();

    float GetAxis(uint32_t index);
    bool GetButton(uint32_t index);

    void SetAxis(uint32_t index, float value);
    void SetButton(uint32_t index, bool state);

    void Update();
};

VirtualJoystick* CreateVirtualJoystick(const VirtualJoystickDesc& desc);
