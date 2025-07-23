# Enable testing
enable_testing()

# Find GTest
find_package(GTest REQUIRED)

# Test source files
set(TEST_SOURCES
    test_organism.cpp
    test_bytecode_vm.cpp
    test_symmetry_analyzer.cpp
    test_evolution_engine.cpp
    test_environment.cpp
)

# Create test executable
add_executable(evosim_tests ${TEST_SOURCES})

# Associate common header for IDE support and dependency tracking.
target_sources(evosim_tests PRIVATE test_common.h)

# Link libraries for tests
target_link_libraries(evosim_tests PRIVATE
    evosim_lib          # Our main library, which brings its PUBLIC dependencies (like OpenCV) with it.
    GTest::gtest        # The Google Test library.
    GTest::gtest_main   # Provides the main() function for the test runner.
    Threads::Threads    # Explicitly link to the threads library, as it's needed by engine tests.
)

# Add tests
add_test(NAME EvoSimTests COMMAND evosim_tests)

# Set test properties
set_tests_properties(EvoSimTests PROPERTIES
    TIMEOUT 300
    ENVIRONMENT "GTEST_COLOR=1"
) 