# For maintainability, automatically gather all .cpp files in this directory
# and its subdirectories. The CONFIGURE_DEPENDS flag ensures that CMake will
# re-run if a new file is added or removed, which is a modern best practice.
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "core/*.cpp" "cli/*.cpp" "utils/*.cpp")

# Define the library target with the automatically gathered source files.
add_library(evosim_lib STATIC ${LIB_SOURCES})

# Also gather corresponding headers to associate with the target for better
# IDE integration (e.g., for tools like CLion or VS Code).
file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/include/core/*.h"
    "${CMAKE_SOURCE_DIR}/include/cli/*.h"
    "${CMAKE_SOURCE_DIR}/include/utils/*.h"
)

# Associate header files with the target.
target_sources(evosim_lib PRIVATE ${LIB_HEADERS})

# Specify the public include directory for our library. Any target that links
# against evosim_lib will automatically have this directory added to its
# include path. This is the modern replacement for include_directories().
target_include_directories(evosim_lib PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

# Set the required C++ standard for this target. This is the modern way to
# ensure compiler compatibility, replacing manual version checks. CMake will
# fail configuration if the selected compiler does not support C++20.
target_compile_features(evosim_lib PUBLIC cxx_std_20)
# Link dependencies. Using PUBLIC propagates these dependencies to targets
# that link against evosim_lib (like our executable and tests).
if(TARGET OpenCV::opencv_core)
    message(STATUS "Linking against namespaced OpenCV::* targets.")
    target_link_libraries(evosim_lib
        PUBLIC
        OpenCV::opencv_core
        OpenCV::opencv_highgui
        OpenCV::opencv_imgproc
        PRIVATE
        Threads::Threads
        nlohmann_json::nlohmann_json
    )
else()
    message(STATUS "Linking against non-namespaced opencv_* targets.")
    target_link_libraries(evosim_lib
        PUBLIC
        opencv_core
        opencv_highgui
        opencv_imgproc
        PRIVATE
        Threads::Threads
        nlohmann_json::nlohmann_json
    )
endif()
# Set compile definitions for the library
target_compile_definitions(evosim_lib PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Set modern, strict compiler flags for our library to enforce code quality.
# These flags are private to the library's compilation.
target_compile_options(evosim_lib PRIVATE
    -Wall                   # Enable a comprehensive set of warnings
    -Wextra                 # Enable even more warnings not covered by -Wall
    -Wpedantic              # Warn on use of non-standard language extensions
    -Wshadow                # Warn when a local variable shadows another
    -Wconversion            # Warn on implicit conversions that may alter a value
    -Wsign-conversion       # Warn for implicit conversions between signed and unsigned integers
    -fstack-protector-strong # Add security checks against stack smashing

    # Treat warnings as errors, but only for Release builds. This prevents
    # development from being blocked by warnings, while ensuring the final
    # production build is clean.
    "$<$<CONFIG:Release>:-Werror>"
)

# Define the main executable.
add_executable(evosim main.cpp)

# Link the executable against our library. It will automatically inherit
# the include paths and link dependencies from evosim_lib.
target_link_libraries(evosim PRIVATE
    evosim_lib
    Boost::program_options
    Boost::filesystem
    Boost::system
)