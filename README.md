# 🕹️ Mapper 🗺️

A simple but powerful Lua script based input remapping tool.

- Create virtual joysticks with any number of axis/buttons
- Fully scripted mapping between any number of inputs and outputs
- Optional GUI mode for configuring and debugging

**NOTE:** This project is in its *earliest stages of prototyping*. Expect a lot of breaking changes and bugs while things take shape.

## Motivation and Goals

There is no standard system for managing input mappings in games. As a result, each new game tends to reinvent the wheel regarding input mapping configuration. And invariably end up with.. less than perfect solutions with a variety of limitations including, but not limited to:

- Controller type misidentification
  - Joystick detected as gamepad (this is more of a device driver issue)
- Deadzones
  - Can't adjust outer deadzones
  - Can't adjust inner deadzones
  - .. or fixed minimum size for inner deadzones
- Actions
  - Can't bind multiple actions to single axis
  - Or vice versa (e.g. mandatory combined throttle/brake)
  - Can't bind keyboard/axis simultaneously
  - Can't bind Axis to momentary (Key/Button) input
- Chording
  - Can't chord keyboard with button input
  - Can't chord keyboard/button with axis input
- Toggle
  - Can't put input into a "toggle" mode
- Layers
  - Can't switch "layers" to map axis onto different controls

## Future Work

- Force feedback routing
- Keyboard and mouse input/emulation support
- Gamepad mapping input/emulation support
- Dynamic script reloading and debugging in GUI mode
- Game detection (detect running applications and load scripts on demand)

## Supported Platforms

- Linux (64 bit)
- Windows (64 bit)

# Building / Installation / Running

Ensure you have CMake, Ninja, and the Clang toolchain installed.

```
$ build.py
    -U    Update dependencies
    -C    Force reconfigure
    -B    Build
```

### Linux

You will also need the development packages for `libevdev` installed on for your distro.

### Windows

On Windows, you must also install vJoy. I test against the latest [BrunnerInnovation/vJoy](https://github.com/BrunnerInnovation/vJoy) signed driver in particular. Mapper will first look for `vJoyInterface.dll` via DLL search, and then look for `C:\Program Files\vJoy\x64\vJoyInterface.dll`

In order to create a virtual joystick in Mapper you must first create a vJoy device with input channels through the vJoy configuration panel, and then specify the vJoy ID (1-16) via `device_id` when creating the virtual joystick.

**NOTE:** This project is not affiliated with any version of vJoy in any way. Use 3rd party applications at your own risk.

# Examples

The `examples` folder contains a set of short, practical mapping scripts that cover the full scripting API.

# Hiding Devices

Often if you're mapping a joystick onto a new virtual device, you want to hide the original from target applications.

### SDL

This is trivial for SDL applications (including anything running under **Proton**, which uses SDL for input) with the following environment variable

`SDL_GAMECONTROLLER_IGNORE_DEVICES=0xVVVV/0xPPPP`

where `VVVV` and `PPPP` are the device's vendor and product ids respectively.

E.g. `SDL_GAMECONTROLLER_IGNORE_DEVICES=0x0483/0x5710`

This can be passed to Steam games that use SDL and/or Proton in the application's Launch Options:

`SDL_GAMECONTROLLER_IGNORE_DEVICES=0x0483/0x5710 %command%`

### HidHide (Windows only)

You can hide devices through the [HidHide](https://github.com/nefarius/HidHide) program. Simply whitelist Mapper and filter any source devices that you don't want applications to see.

**NOTE:** This project is not affiliated with HidHide in any way. Use 3rd party applications at your own risk.
