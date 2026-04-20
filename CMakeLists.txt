set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 4.0.2)
project(Bugmin VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ── Fetched dependencies ──────────────────────────────────────────────────────

FetchContent_Declare(SDL3          GIT_REPOSITORY https://github.com/libsdl-org/SDL.git          GIT_TAG main)
FetchContent_Declare(imgui         GIT_REPOSITORY https://github.com/ocornut/imgui.git            GIT_TAG docking)
FetchContent_Declare(glm           GIT_REPOSITORY https://github.com/g-truc/glm.git               GIT_TAG master)
FetchContent_Declare(spdlog        GIT_REPOSITORY https://github.com/gabime/spdlog.git            GIT_TAG v1.x)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git            GIT_TAG v3.11.2)
FetchContent_Declare(Jolt          GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics           GIT_TAG v5.5.0 SOURCE_SUBDIR Build)
FetchContent_Declare(EnTT          GIT_REPOSITORY https://github.com/skypjack/entt.git            GIT_TAG v3.13.0)
FetchContent_Declare(tinygltf      GIT_REPOSITORY https://github.com/syoyo/tinygltf.git           GIT_TAG release)
FetchContent_Declare(stb           GIT_REPOSITORY https://github.com/nothings/stb.git             GIT_TAG master)
FetchContent_Declare(openal-soft   GIT_REPOSITORY https://github.com/kcat/openal-soft.git         GIT_TAG master)
FetchContent_Declare(enet          GIT_REPOSITORY https://github.com/lsalzman/enet.git            GIT_TAG master)

FetchContent_MakeAvailable(
        SDL3 imgui glm spdlog nlohmann_json
        Jolt EnTT tinygltf stb openal-soft enet
)

# ── ImGui (with both OpenGL3 and SDL-GPU backends) ────────────────────────────

add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PRIVATE SDL3::SDL3)

# ── Main executable ───────────────────────────────────────────────────────────

add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
file(GLOB_RECURSE BUGMIN_SOURCES "src/*.cpp" "src/*.hpp" "src/*.h")
add_executable(Bugmin ${BUGMIN_SOURCES})

target_link_libraries(Bugmin PRIVATE
        SDL3::SDL3
        imgui_lib
        glm::glm
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        Jolt
        EnTT::EnTT
        OpenAL
        enet
)

target_include_directories(Bugmin PRIVATE
        src
        ${stb_SOURCE_DIR}
        ${tinygltf_SOURCE_DIR}
        ${enet_SOURCE_DIR}/include
)

# ── Shader compilation ────────────────────────────────────────────────────────

find_program(GLSLANG_VALIDATOR glslangValidator)
if(GLSLANG_VALIDATOR)
    message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")

    file(GLOB_RECURSE SHADER_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    )

    set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        if(SHADER_NAME MATCHES ".*\\.spv$")
            set(SPIRV_OUT "${SHADER_BINARY_DIR}/${SHADER_NAME}")
            add_custom_command(
                    OUTPUT ${SPIRV_OUT}
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER} ${SPIRV_OUT}
                    DEPENDS ${SHADER}
                    COMMENT "Copying pre-compiled shader ${SHADER_NAME}"
                    VERBATIM
            )
        else()
            set(SPIRV_OUT "${SHADER_BINARY_DIR}/${SHADER_NAME}.spv")
            add_custom_command(
                    OUTPUT ${SPIRV_OUT}
                    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER} -o ${SPIRV_OUT}
                    DEPENDS ${SHADER}
                    COMMENT "Compiling shader ${SHADER_NAME}"
                    VERBATIM
            )
        endif()
        list(APPEND SPIRV_SHADERS ${SPIRV_OUT})
    endforeach()

    add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_SHADERS})
    add_dependencies(Bugmin compile_shaders)

    add_custom_command(TARGET Bugmin POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SHADER_BINARY_DIR}
            $<TARGET_FILE_DIR:Bugmin>/shaders
    )
else()
    message(WARNING "glslangValidator not found! Shaders will not be compiled automatically.")
endif()

# ── Assets & Shader kopieren (immer, unabhängig von glslangValidator) ─────────

add_custom_command(TARGET Bugmin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Assets
        $<TARGET_FILE_DIR:Bugmin>/Assets
)

add_custom_command(TARGET Bugmin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:Bugmin>/shaders
)

# ── Documentation ─────────────────────────────────────────────────────────────

find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN  ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
    )
else()
    add_custom_target(docs
            COMMAND ${CMAKE_COMMAND} -E echo "ERROR: Doxygen not found. Please install it from https://www.doxygen.nl/ and add it to your PATH."
            COMMENT "Doxygen not found - cannot generate documentation"
    )
    message(STATUS "Doxygen not found. 'docs' target will only print an error message.")
endif()

# ── Windows-specific ──────────────────────────────────────────────────────────

if(WIN32)
    target_link_options(Bugmin PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)

    add_custom_command(TARGET Bugmin POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:Bugmin>
    )
    add_custom_command(TARGET Bugmin POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:OpenAL>
            $<TARGET_FILE_DIR:Bugmin>
    )
endif()