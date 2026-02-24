#include "Core/Application.h"
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    try {
        Application app;
        app.Run();
    } catch (const std::exception& e) {
        spdlog::error("An error occured during the booting of the application: {}", e.what());
        return 1;
    }
    return 0;
}