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

function maprange(v, in_low, in_high, out_low, out_high, clamp)
    if clamp == true then
        if v < in_low then return out_low end
        if v > in_high then return out_high end
    end
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

local output = CreateVirtualJoystick {
    name = "Virtual Wheel",
    device_id = 1,
    num_axes = 4,
    num_buttons = 4,
}

local combined_throttle_brake = false
local digital_handbrake = false
local wheel_gamma = 2
local wheel_antideadzone = 0

Register(function()
    local input = FindJoystick(0x0483, 0x5710) -- FrSky Taranis Joystick
    if not input then return end

    local throttle        = max(maprange(input:GetAxis(0), -0.75, 0.929, 0, 1), -0.1)
    local wheel           = deadzone(input:GetAxis(1),  0,    0.071)
    local brake_handbrake = deadzone(input:GetAxis(3),  0.05, 0.120)
    local brake     = max(-brake_handbrake, 0)
    local handbrake = max( brake_handbrake, 0)

    if wheel_gamma ~= 1 then
        wheel = gamma(wheel, wheel_gamma)
    end

    if wheel_antideadzone > 0 then
        wheel = antideadzone(wheel, wheel_antideadzone)
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
        output:SetAxis(3, handbrake * 2)
    end

    local lean = input:GetAxis(2)
    local shoulder = input:GetAxis(4)

    output:SetButton(0, lean >  0.25)
    output:SetButton(1, lean < -0.25)
    output:SetButton(2, shoulder > 0)
end)

function joytowheel(x, y, qmax)
    local r = sqrt(x * x + y * y)
    local q = atan2(x, y)
    return clamp(min(r, 1) * q / qmax, -1, 1)
end

function joytothrottlebreak(x, y, qmax)
    local r = sqrt(x * x + y * y)
    local t = (x > 0 and y > 0) and r or 0
    local b =  x < 0            and r or 0
    local h = (x > 0 and y < 0) and r or 0
    return t, b, h
end

Register(function()
    local input = FindJoystick(0x18d1, 0x9400) -- Google Stadia Controller
    if not input then return end

    local wheel = joytowheel(input:GetAxis(2), -input:GetAxis(3), 2.5)
    local throttle, brake, handbrake = joytothrottlebreak(input:GetAxis(0), -input:GetAxis(1))
    if input:GetButton(9) then handbrake = 1 end

    if wheel_gamma ~= 1 then
        wheel = gamma(wheel, wheel_gamma)
    end

    if wheel_antideadzone > 0 then
        wheel = antideadzone(wheel, wheel_antideadzone)
    end

    output:SetAxis(0, wheel)
    output:SetAxis(1, throttle)
    output:SetAxis(2, brake)
    if digital_handbrake then
        output:SetButton(3, handbrake > 0.3)
    else
        output:SetAxis(3, handbrake)
    end

    local a              = input:GetButton(0)
    local y              = input:GetButton(3)
    local right_shoulder = input:GetButton(10)

    output:SetButton(0, a)
    output:SetButton(1, y)
    output:SetButton(2, right_shoulder)
end)
