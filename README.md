# Mapper

Input Remapping Tool

## Issues with existing game remapping

- Deadzones
  - Can't adjust outer deadzones
  - Can't adjust inner deadzones
  - Minimum sized inner deadzones
- Actions
  - Can't bind multiple actions to single axis
  - Can't bind keyboard/axis simultaneously
- Chording
  - Can't chord keyboard with button input
  - Can't chord keyboard/button with axis input

## Useful scripts

### Disable device from SDL

`SDL_GAMECONTROLLER_IGNORE_DEVICES=0xVVVV/0xPPPP`

where `VVVV` and `PPPP` are the devices vendor and product ids respectively.

E.g. `SDL_GAMECONTROLLER_IGNORE_DEVICES=0x0483/0x5710`

This can be passed to Steam games that use SDL (including any Proton game as Proton uses SDL for input) in the applications Launch Options:

`SDL_GAMECONTROLLER_IGNORE_DEVICES=0x0483/0x5710 %command%`