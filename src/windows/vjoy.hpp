#pragma once

namespace vjoy
{
    namespace api
    {
        using BYTE = unsigned char;
        using BOOL = int;
        using UINT = unsigned int;
        using LONG = long;
        using DWORD = unsigned long;

        struct JOYSTICK_POSITION
        {
            /// JOYSTICK_POSITION
            BYTE    bDevice;	// Index of device. 1-based.

            LONG    wThrottle;
            LONG    wRudder;
            LONG    wAileron;

            LONG    wAxisX;
            LONG    wAxisY;
            LONG    wAxisZ;
            LONG    wAxisXRot;
            LONG    wAxisYRot;
            LONG    wAxisZRot;
            LONG    wSlider;
            LONG    wDial;

            LONG    wWheel;
            LONG    wAccelerator;
            LONG    wBrake;
            LONG    wClutch;
            LONG    wSteering;

            LONG    wAxisVX;
            LONG    wAxisVY;

            LONG    lButtons;	// 32 buttons: 0x00000001 means button1 is pressed, 0x80000000 -> button32 is pressed

            DWORD   bHats;		// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch
            DWORD   bHatsEx1;	// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch
            DWORD   bHatsEx2;	// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch
            DWORD   bHatsEx3;	// Lower 4 bits: HAT switch or 16-bit of continuous HAT switch LONG lButtonsEx1; // Buttons 33-64

            // JOYSTICK_POSITION_V2 Extension
            LONG    lButtonsEx1; // Buttons 33-64
            LONG    lButtonsEx2; // Buttons 65-96
            LONG    lButtonsEx3; // Buttons 97-128

            // JOYSTICK Extension V3: replacing old slots and moving them at the tail
            LONG    wAxisVZ;
            LONG    wAxisVBRX;
            LONG    wAxisVBRY;
            LONG    wAxisVBRZ;
        };

#define VJOY_FUNCTION_LIST \
    VJOY_FUNCTION(vJoyEnabled, BOOL) \
    VJOY_FUNCTION(GetVJDButtonNumber, int, UINT) \
    VJOY_FUNCTION(AcquireVJD, BOOL, UINT) \
    VJOY_FUNCTION(isVJDExists, BOOL, UINT) \
    VJOY_FUNCTION(GetOwnerPid, int, UINT) \
    VJOY_FUNCTION(RelinquishVJD, BOOL, UINT) \
    VJOY_FUNCTION(UpdateVJD, BOOL, UINT, JOYSTICK_POSITION*) \

#define VJOY_FUNCTION(name, ret, ...) inline ret (*name)(__VA_ARGS__);
        VJOY_FUNCTION_LIST
#undef VJOY_FUNCTION
    }

    bool Load();
}
