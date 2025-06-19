abs = math.abs
min = math.min
max = math.max
sqrt = math.sqrt
atan2 = math.atan2

function mag(x, y)
    return sqrt(x ^ 2 + y ^ 2)
end

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

function deadzone_radial(x, y, inner, outer)
    local r = mag(x ,y)
    if r < inner then return 0, 0 end

    local d = deadzone(r, inner, outer)

    -- Bump deadzone output, otherwise value flickers between 1.0 and 0.9999...
    --   due to floating point precision limitations, which leads to rounding issues
    d = d + 0.0000001

    -- Divide through by `r` to normalize vector
    d = d / r

    return d * x, d * y
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
-- local wheel_gamma = 1.2
-- local wheel_antideadzone = 0.01

Register(function()
    local input = FindJoystick(0x0483, 0x5710) -- FrSky Taranis Joystick
    if not input then return end

    local throttle        = max(input:GetAxis(0), -0.1)
    local wheel           = input:GetAxis(1)
    local brake_handbrake = deadzone(input:GetAxis(3), 0.05, 0.120)
    local brake           = max(-brake_handbrake, 0)
    local handbrake       = max(brake_handbrake, 0)

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

    output:SetButton(0, lean > 0.25)
    output:SetButton(1, lean < -0.25)
    output:SetButton(2, shoulder > 0)
end)

function joytowheel(x, y, qmax, r_gamma, q_gamma)
    local r = gamma(min(mag(x, y), 1), r_gamma)
    local q = atan2(x, y) / qmax
    q = clamp(q, -1.0, 1.0)
    q = gamma(q, q_gamma)

    print("r =", r, "q =", q)
    return clamp(min(r, 1) * q, -1, 1)
end

function joytothrottlebreak(x, y, qmax)
    local r = mag(x, y)
    local t = (x > 0 and y > 0) and r or 0
    local b =  x < 0            and r or 0
    local h = (x > 0 and y < 0) and r or 0
    return t, b, h
end

Register(function()
    local input = FindJoystick(0x18d1, 0x9400) -- Google Stadia Controller
    if not input then return end

    local rx, ry = deadzone_radial(input:GetAxis(2), -input:GetAxis(3), 0.13, 0)
    local wheel = joytowheel(rx, ry, 2.5, 2.25, 1.3)
    local lx, ly = deadzone_radial(input:GetAxis(0), -input:GetAxis(1), 0.13, 0)
    local throttle, brake, handbrake = joytothrottlebreak(lx, ly)
    if input:GetButton(9) then handbrake = 1 end
    brake = max(brake, input:GetAxis(5))

    -- if wheel_gamma ~= 1 then
    --     wheel = gamma(wheel, wheel_gamma)
    -- end

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
    output:SetButton(1, right_shoulder)
    output:SetButton(2, y)
end)
