set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 4.0.2)
project(Bugmin VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG main)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG docking)
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG master)
FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.x)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.2)
FetchContent_Declare(Jolt GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics GIT_TAG v5.5.0 SOURCE_SUBDIR Build)
FetchContent_Declare(EnTT GIT_REPOSITORY https://github.com/skypjack/entt.git GIT_TAG v3.13.0)
FetchContent_Declare(tinygltf GIT_REPOSITORY https://github.com/syoyo/tinygltf.git GIT_TAG release)
FetchContent_Declare(stb GIT_REPOSITORY https://github.com/nothings/stb.git GIT_TAG master)
FetchContent_Declare(openal-soft GIT_REPOSITORY https://github.com/kcat/openal-soft.git GIT_TAG master)
FetchContent_Declare(enet GIT_REPOSITORY https://github.com/lsalzman/enet.git GIT_TAG master)
FetchContent_Declare(perlin_noise GIT_REPOSITORY https://github.com/Reputeless/PerlinNoise.git GIT_TAG master)
FetchContent_Declare(voronoi GIT_REPOSITORY https://github.com/JCash/voronoi.git GIT_TAG master)

set(LIBTYPE STATIC CACHE STRING "Build OpenAL as static library to avoid DLL dependency")
set(ALSOFT_BACKEND_PIPEWIRE OFF CACHE BOOL "Disable PipeWire (not available on Windows)")
set(ALSOFT_EXAMPLES OFF CACHE BOOL "Disable examples to avoid symbol conflicts")
set(FETCHCONTENT_QUIET OFF)

FetchContent_MakeAvailable(
        SDL3 imgui glm spdlog nlohmann_json
        Jolt EnTT tinygltf stb openal-soft enet
        perlin_noise
        voronoi
)

add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PRIVATE SDL3::SDL3)

add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
# Force re-run for new source files
file(GLOB_RECURSE BUGMIN_SOURCES "src/*.cpp" "src/*.hpp" "src/*.h")
add_executable(Bugmin ${BUGMIN_SOURCES})
target_compile_options(Bugmin PRIVATE -Wa,-mbig-obj)
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
        ${perlin_noise_SOURCE_DIR}
        ${voronoi_SOURCE_DIR}/src
)

if(WIN32)
    target_compile_definitions(Bugmin PRIVATE AL_LIBTYPE_STATIC)
    # Force stack realignment to 32 bytes — the MinGW CRT only guarantees 16-byte
    # alignment, but the compiler may emit aligned AVX stores (vmovdqa) that
    # require 32-byte alignment. Without this flag the program will crash on
    # startup under Wine and potentially on real Windows too.
    target_compile_options(Bugmin PRIVATE -mstackrealign)
endif()

# Shader Compilation
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
        # If it already ends in .spv (binary), don't append another .spv
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

    # DXBC Shader Compilation (for DirectX 12 backend)
    find_program(SPIRV_CROSS spirv-cross)
    find_program(DXC dxc)
    if(SPIRV_CROSS AND DXC)
        message(STATUS "Found spirv-cross: ${SPIRV_CROSS}")
        message(STATUS "Found dxc: ${DXC}")

        set(DXBC_SHADERS)
        foreach(SPIRV_SHADER ${SPIRV_SHADERS})
            get_filename_component(SHADER_NAME ${SPIRV_SHADER} NAME)
            # Strip .spv to get e.g. "model.vert" from "model.vert.spv"
            string(REGEX REPLACE "\\.spv$" "" SHADER_STEM ${SHADER_NAME})

            # Determine HLSL profile from shader stage in the stem
            if(SHADER_STEM MATCHES "\\.vert$")
                set(HLSL_PROFILE "vs_5_1")
            elseif(SHADER_STEM MATCHES "\\.frag$")
                set(HLSL_PROFILE "ps_5_1")
            else()
                set(HLSL_PROFILE "")
            endif()

            if(HLSL_PROFILE)
                set(HLSL_OUT "${SHADER_BINARY_DIR}/${SHADER_STEM}.hlsl")
                set(DXBC_OUT "${SHADER_BINARY_DIR}/${SHADER_STEM}.dxbc")

                add_custom_command(
                    OUTPUT ${DXBC_OUT}
                    COMMAND ${SPIRV_CROSS} ${SPIRV_SHADER} --hlsl --shader-model 51 --output ${HLSL_OUT}
                    COMMAND ${DXC} -E main -T ${HLSL_PROFILE} ${HLSL_OUT} -Fo ${DXBC_OUT}
                    DEPENDS ${SPIRV_SHADER}
                    COMMENT "Compiling DXBC shader ${SHADER_STEM}"
                    VERBATIM
                )
                list(APPEND DXBC_SHADERS ${DXBC_OUT})
            endif()
        endforeach()

        if(DXBC_SHADERS)
            add_custom_target(compile_dxbc_shaders ALL DEPENDS ${DXBC_SHADERS})
            add_dependencies(compile_dxbc_shaders compile_shaders)
            add_dependencies(Bugmin compile_dxbc_shaders)
        endif()
    else()
        message(WARNING "spirv-cross or dxc not found! DXBC shaders will not be compiled.")
    endif()

    # Copy shaders to executable directory on build
    add_custom_command(TARGET Bugmin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADER_BINARY_DIR}
        $<TARGET_FILE_DIR:Bugmin>/shaders
    )

    # Eigenes "immer ausführen"-Target damit Assets auch ohne C++-Änderung kopiert werden
    add_custom_target(CopyAssets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Assets
        $<TARGET_FILE_DIR:Bugmin>/assets
        COMMENT "Kopiere Assets (src/Assets -> assets/) ..."
    )
    add_dependencies(Bugmin CopyAssets)
else()
    message(WARNING "glslangValidator not found! Shaders will not be compiled automatically.")
endif()

# Documentation
find_package(Doxygen)
if (DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
else()
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E echo "ERROR: Doxygen not found. Please install it from https://www.doxygen.nl/ and add it to your PATH."
        COMMENT "Doxygen not found - cannot generate documentation")
    message(STATUS "Doxygen not found. 'docs' target will only print an error message.")
endif()

if(WIN32)
    # Statically link all runtime libraries (libgcc, libstdc++, winpthread, UCRT)
    # so the exe is fully standalone with no VC redist or other DLL dependencies.
    target_link_options(Bugmin PRIVATE -static)
endif()
