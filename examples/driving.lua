abs = math.abs
min = math.min
max = math.max
sqrt = math.sqrt
atan2 = math.atan2

function clamp(v, l, h)
    if v < l then return l end
    if v > h then return h end
    return v
end

function copysign(v, s)
    return s >= 0 and v or -v
end

function maprange(v, in_low, in_high, out_low, out_high)
    local p = (v - in_low) / (in_high - in_low)
    return p * (out_high - out_low) + out_low
end

function gamma(v, g)
    return copysign(abs(v) ^ g, v)
end

function deadzone(v, inner, outer)
    if abs(v) < inner then return 0 end
    return clamp((v - copysign(inner, v)) / (1 - inner - (outer or 0)), -1, 1)
end

function antideadzone(v, ad)
    if v == 0 then return 0 end
    return (v * (1 - ad)) + copysign(ad, v)
end

function joytowheel(x, y, qmax)
    local r = sqrt(x * x + y * y)
    local q = atan2(x, y)
    return clamp(min(r, 1) * q / qmax, -1, 1)
end

local output = CreateVirtualJoystick {
    name = "Virtual Wheel",
    device_id = 1,
    num_axes = 4,
    num_buttons = 4,
}

local combined_throttle_brake = false
local digital_handbrake = true
local wheel_gamma = true
local wheel_antideadzone = false

Register(function()
    local input = FindJoystick(0x0483, 0x5710) -- FrSky Taranis Joystick
    if not input then return end

    local throttle    = max(deadzone(input:GetAxis(0), 0,    0.041), -0.1)
    local wheel           = deadzone(input:GetAxis(1), 0,    0.071)
    local brake_handbrake = deadzone(input:GetAxis(3), 0.05, 0.110)
    local brake     = max( brake_handbrake, 0)
    local handbrake = max(-brake_handbrake, 0)

    if wheel_gamma then
        wheel = gamma(wheel, 1.5)
    end

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

Register(function()
    local input = FindJoystick(0x18d1, 0x9400) -- Google Stadia Controller
    if not input then return end

    local wheel = joytowheel(input:GetAxis(2), -input:GetAxis(3), 2.5)
    local throttle = max(-input:GetAxis(1), maprange(input:GetAxis(5), -1, 1, 0, 1), 0)
    local brake = max(-input:GetAxis(0), 0)
    local handbrake = input:GetAxis(0) > 0.4

    if wheel_gamma then
        wheel = gamma(wheel, 1.5)
    end

    if wheel_antideadzone then
        wheel = antideadzone(wheel, 0.05)
    end

    output:SetAxis(0, wheel)
    output:SetAxis(1, throttle)
    output:SetAxis(2, brake)
    output:SetButton(3, handbrake)

    local a              = input:GetButton(0)
    local y              = input:GetButton(3)
    local right_shoulder = input:GetButton(10)

    output:SetButton(0, a)
    output:SetButton(1, right_shoulder)
    output:SetButton(2, y)
end)
