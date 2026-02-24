# Bugmin Engine - Starter Template

Welcome to Bugmin! This project is a "Starter Kit" for making games with C++. It handles the difficult setup (creating a window, setting up graphics, and networking) so you can focus on making your game.

---

## What you need to install
If you are new to C++, you will need these tools on your computer:

1.  **A Compiler:** We recommend **MinGW-w64** (specifically the version from [winlibs.com](https://winlibs.com/) or via MSYS2). This is what turns your code into an `.exe` file.
2.  **An IDE:** **CLion** (used to build this project) or **VS Code**. This is where you write your code.
3.  **CMake:** The tool that manages how all the different parts of the project fit together.

---

## Project Structure
Here is a map of the project so you know where everything is:

```text
bugmin/
├── assets/             # Put your images, sounds, and 3D models here.
├── external/           # Code from other people (like GLAD for graphics).
├── src/                # YOUR CODE goes here!
│   ├── Core/           # The "Brain": Handles timing, input (keys/mouse), and the app loop.
│   ├── Gameplay/       # The "Game": This is where you add your players, enemies, and logic.
│   ├── Graphics/       # The "Eyes": Handles drawing things to the screen using OpenGL.
│   ├── Networking/     # The "Phone": Handles connecting to other players online.
│   ├── Window/         # The "Frame": Manages the actual window on your desktop.
│   └── main.cpp        # The starting point of the whole program.
└── CMakeLists.txt      # The "Instruction Manual" for how to build the project.
```

---
### How do I build and run?
If you are using **CLion**:
1.  Open the project folder.
2.  Wait for the "CMake" bar at the bottom to finish.
3.  Click the green **Play** button at the top right.

If you are using the **Command Line**:
1.  Open a terminal in the project folder.
2.  Type: `cmake -B build -G "MinGW Makefiles"`
3.  Type: `cmake --build build`
4.  Run the game: `build/Bugmin.exe`