#!/bin/bash

# EvoSim Build Script
# Builds the evolution simulation system with various options

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_TESTS="ON"
BUILD_EXAMPLES="ON"
CLEAN_BUILD=false
INSTALL=false
VERBOSE=false
JOBS=$(nproc)

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -d, --debug             Build in debug mode"
    echo "  -r, --release           Build in release mode (default)"
    echo "  -c, --clean             Clean build directory before building"
    echo "  -i, --install           Install after building"
    echo "  -t, --no-tests          Disable building tests"
    echo "  -e, --no-examples       Disable building examples"
    echo "  -v, --verbose           Verbose output"
    echo "  -j, --jobs N            Number of jobs for parallel build (default: auto)"
    echo "  --check-deps            Check dependencies"
    echo "  --docker                Build for Docker"
    echo "  --use-clang             Force use of Clang compiler"
    echo "  --use-gcc               Force use of GCC compiler"
    echo ""
    echo "Examples:"
    echo "  $0                      # Build in release mode"
    echo "  $0 -d -c               # Clean debug build"
    echo "  $0 -r -i -j4           # Release build with 4 jobs and install"
    echo "  $0 --check-deps        # Check system dependencies"
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking system dependencies..."
    
    local missing_deps=()
    
    # Check for required packages
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    
    # Check for C++ compilers and recommend the best one
    local gcc_version=""
    local clang_version=""
    
    if command -v g++ &> /dev/null; then
        gcc_version=$(g++ --version | head -n1 | grep -oE '[0-9]+\.[0-9]+' | head -n1)
    fi
    
    if command -v clang++ &> /dev/null; then
        clang_version=$(clang++ --version | head -n1 | grep -oE '[0-9]+\.[0-9]+' | head -n1)
    fi
    
    if [ -z "$gcc_version" ] && [ -z "$clang_version" ]; then
        missing_deps+=("C++ compiler (g++ or clang++)")
    else
        print_status "Found compilers:"
        if [ -n "$gcc_version" ]; then
            print_status "  GCC $gcc_version"
        fi
        if [ -n "$clang_version" ]; then
            print_status "  Clang $clang_version"
        fi
        
        # Recommend the best compiler for C++20
        if [ -n "$clang_version" ] && [ "$(echo "$clang_version >= 10.0" | bc -l 2>/dev/null || echo "0")" = "1" ]; then
            print_success "Recommended: Use Clang $clang_version (best C++20 support)"
            export CC=clang
            export CXX=clang++
        elif [ -n "$gcc_version" ] && [ "$(echo "$gcc_version >= 10.0" | bc -l 2>/dev/null || echo "0")" = "1" ]; then
            print_success "Using GCC $gcc_version (good C++20 support)"
            export CC=gcc
            export CXX=g++
        else
            print_warning "Compiler versions may have limited C++20 support"
        fi
    fi
    
    if ! pkg-config --exists opencv4 2>/dev/null && ! pkg-config --exists opencv 2>/dev/null; then
        missing_deps+=("OpenCV")
    fi
    
    # Check for Boost Libraries (multiple detection methods)
    if ! pkg-config --exists boost 2>/dev/null && [ ! -d "/usr/include/boost" ] && [ ! -d "/usr/local/include/boost" ]; then
        missing_deps+=("Boost Libraries")
    else
        print_status "Boost Libraries found"
    fi
    
    if ! pkg-config --exists gtest 2>/dev/null; then
        missing_deps+=("Google Test")
    fi
    
    if [ ${#missing_deps[@]} -eq 0 ]; then
        print_success "All dependencies are installed"
        return 0
    else
        print_error "Missing dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo ""
        echo "Install missing dependencies:"
        echo "  Ubuntu/Debian: sudo apt-get install build-essential cmake libopencv-dev libboost-all-dev libgtest-dev"
        echo "  CentOS/RHEL: sudo yum install gcc-c++ cmake opencv-devel boost-devel gtest-devel"
        echo "  macOS: brew install cmake opencv boost googletest"
        return 1
    fi
}

# Function to clean build directory
clean_build() {
    if [ -d "build" ]; then
        print_status "Cleaning build directory..."
        rm -rf build
        print_success "Build directory cleaned"
    fi
}

# Function to create build directory
create_build_dir() {
    if [ ! -d "build" ]; then
        print_status "Creating build directory..."
        mkdir -p build
    fi
}

# Function to configure build
configure_build() {
    print_status "Configuring build with CMake..."
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DBUILD_TESTS=$BUILD_TESTS"
        "-DBUILD_EXAMPLES=$BUILD_EXAMPLES"
        "-DOPENCV_ENABLE_NONFREE=ON"
    )
    
    if [ "$VERBOSE" = true ]; then
        cmake_args+=("--debug-output")
    fi
    
    cd build
    cmake "${cmake_args[@]}" ..
    cd ..
    
    print_success "Build configured successfully"
}

# Function to build project
build_project() {
    print_status "Building project with $JOBS jobs..."
    
    cd build
    
    if [ "$VERBOSE" = true ]; then
        make -j"$JOBS" VERBOSE=1
    else
        make -j"$JOBS"
    fi
    
    cd ..
    
    print_success "Build completed successfully"
}

# Function to run tests
run_tests() {
    if [ "$BUILD_TESTS" = "ON" ]; then
        print_status "Running tests..."
        cd build
        ctest --output-on-failure
        cd ..
        print_success "Tests completed"
    fi
}

# Function to install
install_project() {
    if [ "$INSTALL" = true ]; then
        print_status "Installing project..."
        cd build
        sudo make install
        cd ..
        print_success "Installation completed"
    fi
}

# Function to create Docker build
docker_build() {
    print_status "Creating Docker build..."
    
    # Create Docker-specific build
    mkdir -p build-docker
    cd build-docker
    
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_TESTS=OFF \
          -DBUILD_EXAMPLES=OFF \
          -DCMAKE_INSTALL_PREFIX=/app \
          ..
    
    make -j"$JOBS"
    cd ..
    
    print_success "Docker build completed"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -t|--no-tests)
            BUILD_TESTS="OFF"
            shift
            ;;
        -e|--no-examples)
            BUILD_EXAMPLES="OFF"
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        --check-deps)
            check_dependencies
            exit $?
            ;;
        --docker)
            docker_build
            exit 0
            ;;
        --use-clang)
            export CC=clang
            export CXX=clang++
            print_status "Forcing use of Clang compiler"
            shift
            ;;
        --use-gcc)
            export CC=gcc
            export CXX=g++
            print_status "Forcing use of GCC compiler"
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main build process
main() {
    print_status "Starting EvoSim build process..."
    print_status "Build type: $BUILD_TYPE"
    print_status "Build tests: $BUILD_TESTS"
    print_status "Build examples: $BUILD_EXAMPLES"
    print_status "Jobs: $JOBS"
    
    # Check dependencies
    if ! check_dependencies; then
        exit 1
    fi
    
    # Clean build if requested
    if [ "$CLEAN_BUILD" = true ]; then
        clean_build
    fi
    
    # Create build directory
    create_build_dir
    
    # Configure build
    configure_build
    
    # Build project
    build_project
    
    # Run tests
    run_tests
    
    # Install if requested
    install_project
    
    print_success "EvoSim build process completed successfully!"
    
    # Show next steps
    echo ""
    echo "Next steps:"
    echo "  Run the simulator: ./build/bin/evosim --help"
    echo "  Run tests: cd build && ctest --verbose"
    echo "  Install: cd build && sudo make install"
}

# Run main function
main 