#pragma once

#include <format>
#include <chrono>
#include <iostream>

namespace mapper
{
    template<typename... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args)
    {
        auto message = std::vformat(fmt.get(), std::make_format_args(args...));
        std::cout << message << '\n';
        throw std::runtime_error(message);
    }

    template<typename... Args>
    void Log(std::format_string<Args...> fmt, Args&&... args)
    {
        std::cout << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
    }

    inline
    std::string DurationToString(std::chrono::duration<double, std::nano> dur)
    {
        double nanos = dur.count();

        constexpr auto deciamls_for_3sf = [](double value)
        {
            if (value < 10) return 2;
            if (value < 100) return 1;
            return 0;
        };

        if (nanos >= 1e9) {
            double seconds = nanos / 1e9;
            return std::format("{:.{}f}s", seconds, deciamls_for_3sf(seconds));
        }

        if (nanos >= 1e6) {
            double millis = nanos / 1e6;
            return std::format("{:.{}f}ms", millis, deciamls_for_3sf(millis));
        }

        if (nanos >= 1e3) {
            double micros = nanos / 1e3;
            return std::format("{:.{}f}us", micros, deciamls_for_3sf(micros));
        }

        if (nanos >= 0) {
            return std::format("{:.{}f}ns", nanos, deciamls_for_3sf(nanos));
        }

        return "0";
    }
}
