/**
 * @file main.cpp
 * @brief Entry point for the Bugmin application.
 */

#include "Core/Application.h"
#include <spdlog/spdlog.h>

/**
 * @brief Main entry point of the application.
 * 
 * Initializes the Application class and starts the main loop.
 * Catches and logs any top-level exceptions.
 * 
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return int Exit code (0 for success, 1 for failure).
 */
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
