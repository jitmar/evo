# EvoSim - Evolution Simulator

A sophisticated C++ software system for simulating the evolution of programmable virtual organisms in a containerized environment.

## Overview

EvoSim is an industry-grade evolution simulation system that creates a virtual environment where self-replicating programs (organisms) evolve through natural selection. Each organism contains bytecode that generates visual patterns, which are evaluated for symmetry to determine fitness scores.

## Features

- **Self-Replicating Organisms**: Virtual organisms that can replicate themselves with random mutations
- **Bytecode Virtual Machine**: Custom VM for executing organism programs to generate images
- **Symmetry Analysis**: Advanced image analysis to evaluate organism fitness based on symmetry patterns
- **Evolutionary Algorithm**: Natural selection, mutation, and population management
- **Comprehensive CLI**: Full command-line interface for controlling the simulation
- **Containerized Deployment**: Docker support for isolated, reproducible environments
- **Real-time Monitoring**: Live statistics and progress tracking
- **State Persistence**: Save/load simulation states
- **Configurable Parameters**: Extensive configuration options for all aspects of the simulation

## Architecture

### Core Components

1. **Organism**: Self-replicating entities with bytecode and metadata
2. **BytecodeVM**: Virtual machine for executing organism programs
3. **SymmetryAnalyzer**: Image analysis for fitness evaluation
4. **Environment**: Virtual ecosystem managing organism populations
5. **EvolutionEngine**: Orchestrates the evolution process
6. **CommandProcessor**: CLI command handling
7. **EvolutionController**: High-level system control

### System Design

- **Thread-safe**: Multi-threaded architecture with proper synchronization
- **Modular**: Clean separation of concerns with well-defined interfaces
- **Extensible**: Plugin-like architecture for easy customization
- **Robust**: Comprehensive error handling and logging
- **Performant**: Optimized for large-scale simulations

## Quick Start

### Prerequisites

- **C++20 compatible compiler**:
  - **Clang 10+** (recommended - best C++20 support)
  - **GCC 10+** (good C++20 support, some features may be limited)
  - **MSVC 2019 16.11+** (good C++20 support)
- CMake 3.20+
- OpenCV 4.8+
- Boost Libraries 1.82+
- Docker (for containerized deployment)

### Building from Source

```bash
# Clone the repository
git clone <repository-url>
cd evosim

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Or use the build script (recommended)
./scripts/build.sh --use-clang  # Use Clang for best C++20 support

# Run tests
make test

# Install (optional)
sudo make install
```

### Docker Deployment

```bash
# Build the container
docker build -t evosim .

# Run interactively
docker run -it --rm evosim

# Run as daemon
docker-compose --profile daemon up -d

# Run CLI commands
docker-compose --profile cli run evosim-cli --help
```

### Basic Usage

```bash
# 1. Start the server daemon in a terminal
./build/bin/evosimd --config my_config.yaml

# 2. In another terminal, send commands with the client

# Get the current status
./build/bin/evosim status

# Load a state from a file
./build/bin/evosim load --file save_001.json

```

## Configuration

The system uses a YAML configuration file (`evosim.yaml`) with sections for different components:

```yaml
environment:
  initial_population: 100
  max_population: 1000
  mutation_rate: 0.01

bytecode_vm:
  image_width: 256
  image_height: 256

symmetry_analyzer:
  horizontal_weight: 0.25
  vertical_weight: 0.25
```

## CLI Commands

### Core Commands

- `start` - Start evolution simulation
- `stop` - Stop evolution simulation
- `pause` - Pause evolution simulation
- `resume` - Resume evolution simulation
- `status` - Show simulation status
- `stats` - Show detailed statistics

### Management Commands

- `config` - Show/modify configuration
- `save` - Save current state
- `load` - Load state from file
- `export` - Export evolution data
- `organism` - Manage individual organisms
- `population` - Manage organism populations

### Utility Commands

- `help` - Show help information
- `clear` - Clear screen/history
- `exit` - Exit the program

## API Reference

### Organism Class

```cpp
class Organism {
public:
    OrganismPtr replicate(double mutation_rate = 0.01) const;
    double getFitnessScore() const;
    const Bytecode& getBytecode() const;
    // ... more methods
};
```

### Environment Class

```cpp
class Environment {
public:
    bool update();  // Run one generation
    double evaluateFitness(const OrganismPtr& organism);
    std::vector<OrganismPtr> selectForReproduction(uint32_t count);
    // ... more methods
};
```

### EvolutionEngine Class

```cpp
class EvolutionEngine {
public:
    bool start();
    bool stop();
    bool pause();
    bool resume();
    EngineStats getStats() const;
    // ... more methods
};
```

## Development

### Project Structure

```
evosim/
├── include/           # Header files
│   ├── core/         # Core simulation components
│   ├── cli/          # Command-line interface
│   └── utils/        # Utility classes
├── src/              # Source files
├── tests/            # Unit tests
├── config/           # Configuration files
├── docs/             # Documentation
├── scripts/          # Build and deployment scripts
└── docker/           # Docker configuration
```

### Building and Testing

```bash
# Build with tests
cmake -DBUILD_TESTS=ON ..
make

# Or use the automated build script
./scripts/build.sh --use-clang -t  # Use Clang with tests
./scripts/build.sh --use-gcc -c    # Use GCC with clean build
```

### Compiler Selection

For the best C++20 experience, **Clang is recommended**:

```bash
# Install Clang (Ubuntu/Debian)
sudo apt-get install clang-15 libc++-15-dev libc++abi-15-dev

# Install Clang (CentOS/RHEL)
sudo yum install clang libcxx-devel

# Install Clang (macOS)
brew install llvm

# Use Clang for building
export CC=clang
export CXX=clang++
cmake ..
make
```

# Run tests
ctest --verbose

# Build with debug information
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Code Style

- Follow Google C++ Style Guide
- Use meaningful variable and function names
- Add comprehensive documentation
- Write unit tests for all components
- Use RAII and smart pointers
- Prefer const correctness

## Performance

### Optimization Tips

1. **Population Size**: Adjust based on available memory
2. **Image Resolution**: Lower resolution for faster processing
3. **Generation Time**: Balance between speed and detail
4. **Threading**: Utilize multiple CPU cores
5. **Caching**: Enable random number caching
6. **Memory Management**: Use appropriate data structures

### Benchmarks

- **Small Population** (100 organisms): ~1 generation/second
- **Medium Population** (1000 organisms): ~0.5 generations/second
- **Large Population** (10000 organisms): ~0.1 generations/second

## Troubleshooting

### Common Issues

1. **Build Failures**: Ensure all dependencies are installed
2. **Memory Issues**: Reduce population size or image resolution
3. **Performance**: Enable optimizations and use appropriate hardware
4. **Docker Issues**: Check Docker daemon and permissions

### Debug Mode

```bash
# Enable debug logging
./evosim --log-level debug --debug

# Run with verbose output
./evosim --verbose

# Check system requirements
./evosim --check-system
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

### Development Setup

```bash
# Install development dependencies
sudo apt-get install build-essential cmake git
sudo apt-get install libopencv-dev libboost-all-dev

# Clone and setup
git clone <repository-url>
cd evosim
mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
make
```

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- OpenCV for computer vision capabilities
- Boost Libraries for C++ utilities
- CMake for build system
- Docker for containerization

## Support

For support and questions:
- Create an issue on GitHub
- Check the documentation
- Review the configuration examples
- Consult the troubleshooting guide
