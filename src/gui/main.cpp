#include <filesystem>
#include <optional>

#include "application.hpp"
#include "utils/cmdlib.hpp"

int main(int argc, char **argv)
{
    std::optional<std::filesystem::path> startup_model;
    const auto args = get_native_args(argc, argv);
    if (args.size() > 1)
    {
        startup_model = args[1];
    }

    gui::Application app;
    return app.run(startup_model);
}
