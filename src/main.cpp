#include "mapper.hpp"

// -----------------------------------------------------------------------------

struct ProgramArgs
{
    bool gui = false;
    std::vector<std::filesystem::path> initial_script_paths;
};

static
ProgramArgs ParseArgs(int argc, char* argv[])
{
    ProgramArgs args;

    if (argc > 1 && std::string_view(argv[1]) == "--gui") {
        args.gui = true;
    }

    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);
        if (arg == "--gui") args.gui = true;
        else {
            auto path = std::filesystem::path(arg);
            path = std::filesystem::canonical(path);

            if (!std::filesystem::exists(path)) {
                Error("Error: could not find script file: {}", path.string());
            }

            args.initial_script_paths.emplace_back(path);
        }
    }

    return args;
}

// -----------------------------------------------------------------------------

static
int Main(int argc, char* argv[]) try
{
    auto args = ParseArgs(argc, argv);
    Initialize();
    for (auto& script_path : args.initial_script_paths) {
        LoadScript(script_path);
    }
    if (args.gui) OpenGUI();

    while (ProcessEvents()) {
        UpdateJoysticks();
    }

    if (args.gui) CloseGUI();

    return EXIT_SUCCESS;
}
catch (const std::exception& e)
{
    Log("Exception: {}", e.what());
    return EXIT_FAILURE;
}
catch (...)
{
    Log("Uncaught Exception");
    return EXIT_FAILURE;
}

// -----------------------------------------------------------------------------

#if WIN32

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <Windows.h>

int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Main(__argc, __argv);
}

#endif

int main(int argc, char* argv[])
{
    Main(argc, argv);
}
