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
