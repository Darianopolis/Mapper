function clamp(v, l, h)
    if v < l then return l end
    if v > h then return h end
    return v
end

function copysign(v, s)
    return s >= 0 and v or -v
end

function abs(v)
    return v >= 0 and v or -v
end

function min(a, b)
    return a <= b and a or b
end

function max(a, b)
    return a >= b and a or b
end

function deadzone(v, inner, outer)
    if abs(v) < inner then return 0 end
    return clamp((v - copysign(inner, v)) / (1 - inner - (outer or 0)), -1, 1)
end

function antideadzone(v, ad)
    if v == 0 then return v end
    return (v * (1 - ad)) + copysign(ad, v)
end

local output = CreateVirtualJoystick {
    name = "Virtual Wheel",
    device_id = 1,
    num_axes = 4,
    num_buttons = 4,
}

local combined_throttle_brake = false
local digital_handbrake = true
local wheel_antideadzone = false

Register(function()
    local input = FindJoystick(0x0483, 0x5710)
    if not input then return end

    local throttle    = max(deadzone(input:GetAxis(0), 0,    0.041), -0.1)
    local wheel           = deadzone(input:GetAxis(1), 0,    0.071)
    local brake_handbrake = deadzone(input:GetAxis(3), 0.05, 0.11 )
    local brake     = max( brake_handbrake, 0)
    local handbrake = max(-brake_handbrake, 0)

    if wheel_antideadzone then
        wheel = antideadzone(wheel, 0.05)
    end

    if combined_throttle_brake then
        throttle = brake > 0 and -brake or max(throttle, 0)
        brake = 0
    end

    output:SetAxis(0, wheel)
    output:SetAxis(1, throttle)
    output:SetAxis(2, brake)
    if digital_handbrake then
        output:SetButton(3, handbrake > 0.3)
    else
        output:SetAxis(3, handbrake)
    end

    local lean = input:GetAxis(2)
    local shoulder = input:GetAxis(4)

    output:SetButton(0, lean >  0.25)
    output:SetButton(1, lean < -0.25)
    output:SetButton(2, shoulder > 0)
end)
