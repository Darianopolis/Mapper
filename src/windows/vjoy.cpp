#include "vjoy.hpp"

#include <mapper.hpp>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace vjoy
{
    bool Load()
    {
        static bool loaded = false;
        if (loaded) return true;

        auto dll = ::LoadLibraryA("vJoyInterface.dll");

        if (!dll) {
            dll = ::LoadLibraryA("C:\\Program Files\\vJoy\\x64\\vJoyInterface.dll");

            if (!dll) {
                Log("vJoyInterface.dll could not be located. Ensure you have installed vJoy correctly. The following locations are searched");
                Log(" - vJoyInterface.dll");
                Log(" - C:\\Program Files\\vJoy\\x64\\vJoyInterface.dll");
                Log("Add the 'x64' folder to your path if installing vJoy anywhere other than C:\\Program Files\\vJoy");

                return false;
            }
        }

        using namespace vjoy::api;

#define VJOY_FUNCTION(name, ...) \
        name = std::bit_cast<decltype(name)>(::GetProcAddress(dll, #name)); \
        if (!name) { Log("[vJoy] Failed to load vjoy::api::" #name); return false; }
        VJOY_FUNCTION_LIST
#undef VJOY_FUNCTION

        loaded = true;
        return true;
    };
}
