function clamp(v, l, h)
    if v < l then return l end
    if v > h then return h end
    return v
end

local output = CreateVirtualJoystick {
    name = "Virtual Wheel",
    num_axes = 4,
    num_buttons = 4,
}

local combined_throttle_brake = false
local digital_handbrake = true

Register(function()
    local input = FindJoystick(0x0483, 0x5710)
    if not input then return end

    local throttle  = clamp(input:GetAxis(0) * 1.04158, -0.1, 1)
    local wheel     = clamp(input:GetAxis(1) * 1.06700, -1.0, 1)
    local brake_handbrake = input:GetAxis(3)
    local brake     = clamp( brake_handbrake * 1.09529, 0, 1)
    local handbrake = clamp(-brake_handbrake * 1.09529, 0, 1)

    if combined_throttle_brake then
        throttle = clamp(throttle, 0, 1)
        if (brake > 0.05) then
            throttle = -(brake - 0.05) / 0.95
        end
        brake = 0
    else
        brake = clamp((brake - 0.05) / 0.95, 0, 1)
    end

    output:SetAxis(0, wheel)
    output:SetAxis(1, throttle)
    output:SetAxis(2, brake)
    if digital_handbrake then
        output:SetButton(3, handbrake > 0.35)
    else
        output:SetAxis(3, brake)
    end

    local lean = input:GetAxis(2)
    local shoulder = input:GetAxis(4)

    output:SetButton(0, lean > 0.25)
    output:SetButton(1, lean < -0.25)
    output:SetButton(2, shoulder > 0)
end)
