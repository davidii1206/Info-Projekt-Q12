# Info-Projekt-Q12

This guide assumes you have nothing installed yet. We will use JetBrains CLion because it handles the heavy lifting of C++ environments for you.

1. Install the Essentials
Before opening the code, you need a compiler (the tool that turns text into a program).

Windows: Download and install Visual Studio Community. During installation, check the box: "Desktop development with C++". (This provides the MSVC compiler).

macOS: Open your terminal and type xcode-select --install.

Linux (Ubuntu/Debian): Run sudo apt update && sudo apt install build-essential git.

2. Get CLion
Download and install JetBrains CLion.

Open CLion and follow the setup wizard. It should automatically detect the compiler you installed in Step 1 (look for "Toolchains" in settings if it doesn't).

3. Setup the Project
Clone or Download this repository to a folder on your computer.

Open CLion and select "Open". Navigate to the folder and select the CMakeLists.txt file.

When prompted, click "Open as Project".

4. The Build
Once the project is open, look at the bottom status bar in CLion. You will see "CMake" loading.

Wait: Bugmin uses FetchContent. This means CLion is currently downloading all libraries (SDL3, WebGPU, ImGui, etc.) directly from GitHub. This might take 2-5 minutes depending on your internet.

Success: When the console says CMake generation finished, you are ready.

5. Run Bugmin
In the top right corner of CLion, ensure the target Bugmin is selected in the dropdown.

Click the Green Play Button (or press Shift + F10).

The engine will compile and launch a window.
