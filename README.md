# 🕹️ Mapper 🗺️

A simple, Lua script based input remapping tool intended to simplify the process of configuring joystick devices with applications regardless of their native configuration capabilities (or lack thereof)

**NOTE:** This project is in its *earliest stages of prototyping*. Expect a lot of breaking changes and bugs while things take shape.

## Features

- Create virtual joysticks with any number of axis/buttons
- Fully scripted mapping between any number of inputs and outputs
- **Optional** GUI mode for configuring and debugging

## Motivation and Goals

There is no standard system for managing input mappings in games. As a result, each new game tends to reinvent the wheel regarding input mapping configuration. And invariably end up with.. less than perfect solutions with a variety of limitations including but not limitated to:

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

## Supported Platforms

- Linux

## To Do

- Keyboard and mouse emulation
- Gamepad mapping and emulation support
- Dynamic script reloading and debugging in GUI mode
- Game detection (detect running applications and load scripts on demand)

## Building and Running

Ensure you have the development packages for `libevdev` installed on for your distro, along with `git` (duh), `cmake`, `ninja` and `clang`.

```
$ build.py
    -U    Update dependencies
    -C    Force reconfigure
    -B    Build
```

You must also run the following to install a `udev` rule to enable device access for created virtual devices. This will require root privileges.

```
# rules/update-rules.sh
```

## Examples

The `examples` folder contains a set of short, practical mapping scripts that cover the full scripting API.

## Useful

### Disable device from SDL

Often if you're mapping a joystick onto a new virtual device, you want to hide the original from target applications.

This is trivial for SDL applications (including anything running under **Proton**, which uses SDL for input) with the following environment variable

`SDL_GAMECONTROLLER_IGNORE_DEVICES=0xVVVV/0xPPPP`

where `VVVV` and `PPPP` are the device's vendor and product ids respectively.

E.g. `SDL_GAMECONTROLLER_IGNORE_DEVICES=0x0483/0x5710`

This can be passed to Steam games that use SDL and/or Proton in the application's Launch Options:

`SDL_GAMECONTROLLER_IGNORE_DEVICES=0x0483/0x5710 %command%`