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

# Link libraries for tests
target_link_libraries(evosim_tests
    GTest::gtest
    GTest::gtest_main
    ${OpenCV_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
)

# Add tests
add_test(NAME EvoSimTests COMMAND evosim_tests)

# Set test properties
set_tests_properties(EvoSimTests PROPERTIES
    TIMEOUT 300
    ENVIRONMENT "GTEST_COLOR=1"
) 