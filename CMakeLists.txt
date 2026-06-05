cmake_minimum_required(VERSION 3.16)

if(WIN32 AND CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    set(_MSYS2_PREFIXES
        "$ENV{MINGW_PREFIX}"
        "C:/msys64/ucrt64"
        "C:/msys64/mingw64"
        "C:/msys64/mingw32"
    )

    find_program(CMAKE_MAKE_PROGRAM
        NAMES mingw32-make mingw32-make.exe
        PATHS ${_MSYS2_PREFIXES}
        PATH_SUFFIXES bin
        DOC "MinGW make program"
    )

    if(NOT CMAKE_MAKE_PROGRAM)
        message(FATAL_ERROR
            "The 'MinGW Makefiles' generator needs mingw32-make.exe, but it was not found. "
            "Install it from MSYS2 with: pacman -S --needed mingw-w64-ucrt-x86_64-make "
            "Then run CMake again from a shell where C:/msys64/ucrt64/bin is on PATH."
        )
    endif()

    if(NOT CMAKE_C_COMPILER)
        find_program(CMAKE_C_COMPILER
            NAMES gcc gcc.exe
            PATHS ${_MSYS2_PREFIXES}
            PATH_SUFFIXES bin
            DOC "MinGW C compiler"
        )
    endif()

    if(NOT CMAKE_CXX_COMPILER)
        find_program(CMAKE_CXX_COMPILER
            NAMES g++ g++.exe
            PATHS ${_MSYS2_PREFIXES}
            PATH_SUFFIXES bin
            DOC "MinGW C++ compiler"
        )
    endif()
endif()

project(MyVulkanApp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

find_program(SLANGC_EXECUTABLE
    NAMES slangc slangc.exe
    HINTS
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{VULKAN_SDK}/Bin"
        "${Vulkan_INCLUDE_DIR}/../bin"
        "${Vulkan_INCLUDE_DIR}/../Bin"
    DOC "Slang shader compiler"
)

if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR
        "slangc was not found. Install Slang or use a Vulkan SDK that includes it, "
        "then add its bin directory to PATH or set SLANGC_EXECUTABLE."
    )
endif()

function(add_slang_shader_target TARGET)
    cmake_parse_arguments(
        SHADER
        ""
        "OUTPUT"
        "SOURCES;ENTRY_POINTS"
        ${ARGN}
    )

    if(NOT SHADER_SOURCES)
        message(FATAL_ERROR "${TARGET}: no Slang shader sources were provided")
    endif()

    if(NOT SHADER_ENTRY_POINTS)
        set(SHADER_ENTRY_POINTS vertMain fragMain)
    endif()

    if(NOT SHADER_OUTPUT)
        set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${TARGET}.spv")
    elseif(NOT IS_ABSOLUTE "${SHADER_OUTPUT}")
        set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_OUTPUT}")
    endif()

    get_filename_component(SHADER_OUTPUT_DIR "${SHADER_OUTPUT}" DIRECTORY)

    set(SHADER_ENTRY_POINT_ARGUMENTS)
    foreach(ENTRY_POINT IN LISTS SHADER_ENTRY_POINTS)
        list(APPEND SHADER_ENTRY_POINT_ARGUMENTS -entry "${ENTRY_POINT}")
    endforeach()

    add_custom_command(
        OUTPUT "${SHADER_OUTPUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
        COMMAND "${SLANGC_EXECUTABLE}"
                ${SHADER_SOURCES}
                -target spirv
                -profile spirv_1_4
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                ${SHADER_ENTRY_POINT_ARGUMENTS}
                -o "${SHADER_OUTPUT}"
        DEPENDS ${SHADER_SOURCES}
        COMMENT "Compiling Slang shaders for ${TARGET}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target("${TARGET}" DEPENDS "${SHADER_OUTPUT}")
endfunction()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate GLFW install target" FORCE)

if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "Build GLFW X11 backend" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build GLFW Wayland backend" FORCE)
endif()

set(GLFW_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/glfw")
if(NOT EXISTS "${GLFW_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "GLFW was not found at ${GLFW_SOURCE_DIR}")
endif()

add_subdirectory("${GLFW_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/external/glfw")

set(FMT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/fmt")
if(EXISTS "${FMT_SOURCE_DIR}/CMakeLists.txt")
    set(FMT_DOC OFF CACHE BOOL "Build fmt documentation" FORCE)
    set(FMT_TEST OFF CACHE BOOL "Build fmt tests" FORCE)
    set(FMT_INSTALL OFF CACHE BOOL "Generate fmt install target" FORCE)
    add_subdirectory("${FMT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/external/fmt")
endif()

add_executable(my_vulkan_app
    main.cpp
    Application.cpp
    Application.hpp
)

set(SHADER_SLANG_SOURCES
    "${PROJECT_SOURCE_DIR}/shaders/src/base_shader.slang"
)

add_slang_shader_target(compile_slang_shaders
    SOURCES ${SHADER_SLANG_SOURCES}
    ENTRY_POINTS vertMain fragMain
    OUTPUT "${PROJECT_SOURCE_DIR}/shaders/compiled/slang.spv"
)

add_dependencies(my_vulkan_app compile_slang_shaders)

target_compile_options(my_vulkan_app PRIVATE
    -Wall
    -Wextra
    -Werror
)

target_include_directories(my_vulkan_app PRIVATE
    "${PROJECT_SOURCE_DIR}"
)

target_compile_definitions(my_vulkan_app PRIVATE
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

target_link_libraries(my_vulkan_app PRIVATE
    Vulkan::Vulkan
    glfw
    $<$<TARGET_EXISTS:fmt::fmt>:fmt::fmt>
)
