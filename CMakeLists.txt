cmake_minimum_required(VERSION 3.20)
project(EvoSim VERSION 0.1.0 LANGUAGES CXX)

# Some environments (like VS Code Kits) specify a toolchain file automatically.
# By referencing CMAKE_TOOLCHAIN_FILE here, we acknowledge its use and prevent
# CMake from issuing a warning about an unused manually-specified variable.
# This variable is implicitly used by the 'project()' command to select the compiler.
if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_TOOLCHAIN_FILE STREQUAL "")
    message(STATUS "A toolchain file is being used: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# =============================================================================
#  Project Options
# =============================================================================
option(BUILD_TESTS "Build the project's test suite." ON)
option(BUILD_EXAMPLES "Build the project's examples." ON)

# Set C++ standard globally. We will enforce it on a per-target basis using
# modern CMake features, which is more robust than manual version checks.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# On many Linux systems, pre-compiled libraries (like Boost and GTest) are
# built with GCC's libstdc++. If we compile our code with Clang, which may
# default to libc++, this can cause "undefined reference" linker errors due to
# an Application Binary Interface (ABI) incompatibility.
#
# To resolve this for all targets in the project (libraries, executables, and
# tests), we globally add the `-stdlib=libstdc++` flag when using Clang.
# This does NOT reduce C++20 language conformance, as Clang still provides the
# language features. The C++20 library features will be provided by the
# system's (modern) libstdc++, which has robust C++20 support.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Clang compiler detected. Forcing -stdlib=libstdc++ for ABI compatibility.")
    add_compile_options(-stdlib=libstdc++)
    # The link option is also needed to ensure the linker uses the correct standard library.
    add_link_options(-stdlib=libstdc++)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags are set on a per-target basis in the subdirectories (e.g., src/CMakeLists.txt)
# using target_compile_options(). This is more robust than setting global CMAKE_CXX_FLAGS.
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Find required packages
message(STATUS "--- Debugging OpenCV ---")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "Searching for OpenCV...")
find_package(OpenCV 4.12.0 REQUIRED COMPONENTS core highgui imgproc)

# After find_package, print the variables it found to see what it's using.
message(STATUS "OpenCV_FOUND: ${OpenCV_FOUND}")
message(STATUS "OpenCV_DIR: ${OpenCV_DIR}")
message(STATUS "OpenCV_VERSION: ${OpenCV_VERSION}")
message(STATUS "--- End Debugging ---")

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options filesystem system)
find_package(nlohmann_json 3.2.0 REQUIRED)

# Verify that the modern OpenCV targets were created.
if(NOT TARGET opencv_core AND NOT TARGET OpenCV::opencv_core)
    message(FATAL_ERROR "OpenCV was found, but it did not provide the modern 'opencv_core' or 'OpenCV::opencv_core' target. Your OpenCV installation may be misconfigured or the install step was incomplete.")
endif()
# The message below is commented out as it uses an old-style variable.
# message(STATUS "Using Boost ${Boost_VERSION}")

# Add subdirectories
add_subdirectory(src)

if(BUILD_TESTS)
  # This enables CTest support
  enable_testing()
  # This tells CMake to process the CMakeLists.txt in the 'tests' directory
  add_subdirectory(tests)
endif()

# if(BUILD_EXAMPLES)
#   add_subdirectory(examples)
# endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Install rules
# The install rule for the 'evosim' executable belongs in src/CMakeLists.txt,
# where the target is defined.

install(DIRECTORY include/ DESTINATION include) 