# Source files for library (excluding main.cpp)
set(LIB_SOURCES
    core/organism.cpp
    core/environment.cpp
    core/bytecode_vm.cpp
    core/symmetry_analyzer.cpp
    core/evolution_engine.cpp
    cli/command_processor.cpp
    cli/evolution_controller.cpp
    utils/logger.cpp
    utils/config_manager.cpp
    utils/random_generator.cpp
)

# Header files
set(HEADERS
    core/organism.h
    core/environment.h
    core/bytecode_vm.h
    core/symmetry_analyzer.h
    core/evolution_engine.h
    cli/command_processor.h
    cli/evolution_controller.h
    utils/logger.h
    utils/config_manager.h
    utils/random_generator.h
)

# Create library
add_library(evosim_lib STATIC ${LIB_SOURCES})

# Link libraries for the library
target_link_libraries(evosim_lib
    ${OpenCV_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
)

# Set compile definitions for the library
target_compile_definitions(evosim_lib PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Create executable that links to the library
add_executable(evosim main.cpp)

# Link the executable to the library
target_link_libraries(evosim evosim_lib) 