cmake_minimum_required(VERSION 3.20)
project(EvoSim VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check compiler C++20 support
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR "GCC 10.0 or later required for C++20 support. Found: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    message(STATUS "Using GCC ${CMAKE_CXX_COMPILER_VERSION} for C++20")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR "Clang 10.0 or later required for C++20 support. Found: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    message(STATUS "Using Clang ${CMAKE_CXX_COMPILER_VERSION} for C++20")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.28")
        message(FATAL_ERROR "MSVC 2019 16.11 or later required for C++20 support. Found: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    message(STATUS "Using MSVC ${CMAKE_CXX_COMPILER_VERSION} for C++20")
else()
    message(WARNING "Unknown compiler ${CMAKE_CXX_COMPILER_ID}. C++20 support may be limited.")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Find required packages
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# Find Boost with fallback options
find_package(Boost QUIET COMPONENTS program_options filesystem system)
if(NOT Boost_FOUND)
    # Try without components first
    find_package(Boost REQUIRED)
    if(Boost_FOUND)
        message(STATUS "Boost found without specific components")
    endif()
else()
    message(STATUS "Boost found with required components")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Install rules
install(TARGETS evosim
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include) 