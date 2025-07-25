#!/usr/bin/env python3
"""
Simple Evolution Example
Demonstrates basic usage of the EvoSim evolution simulation system.
"""

import subprocess
import sys
import time
import json
import os

def run_command(cmd, capture_output=True):
    """Run a command and return the result."""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=capture_output, text=True)
        return result.returncode == 0, result.stdout, result.stderr
    except Exception as e:
        return False, "", str(e)

def check_system():
    """Check if the evolution system is available."""
    print("Checking EvoSim system...")
    
    # Check if executables exist
    if not os.path.exists("./build/bin/evosim"):
        print("❌ EvoSim client executable not found. Please build the project first:")
        print("   ./scripts/build.sh")
        return False
    if not os.path.exists("./build/bin/evosimd"):
        print("❌ EvoSim daemon executable not found. Please build the project first:")
        print("   ./scripts/build.sh")
        return False
    
    # Check version
    success, output, error = run_command("./build/bin/evosim --version")
    if success:
        print(f"✅ EvoSim found: {output.strip()}")
    else:
        print(f"❌ Failed to get version: {error}")
        return False
    
    return True

def create_config():
    """Create a simple configuration for the example."""
    # This configuration is now in YAML format.
    config = """
environment:
  initial_population: 50
  max_population: 200
  mutation_rate: 0.02
  generation_time_ms: 500

bytecode_vm:
  image_width: 128
  image_height: 128
  max_instructions: 5000

symmetry_analyzer:
  horizontal_weight: 0.3
  vertical_weight: 0.3
  diagonal_weight: 0.2
  rotational_weight: 0.2

evolution_engine:
  save_interval_generations: 50
"""
    
    with open("example_config.yaml", "w") as f:
        f.write(config)
    
    print("✅ Created example configuration: example_config.yaml")

def run_simple_evolution():
    """Run a simple evolution simulation."""
    print("\n🚀 Starting simple evolution simulation...")
    
    # Start evolution
    success, output, error = run_command("./build/bin/evosim --config example_config.yaml start")
    
    if not success:
        print(f"❌ Failed to start evolution: {error}")
        return False
    
    print("✅ Evolution started successfully")
    
    # Let it run for a few generations
    print("⏳ Running for 10 generations...")
    time.sleep(5)
    
    # Check status
    success, output, error = run_command("./build/bin/evosim --config example_config.yaml status")
    
    if success:
        print("📊 Current status:")
        print(output)
    else:
        print(f"❌ Failed to get status: {error}")
    
    # Stop evolution
    success, output, error = run_command("./build/bin/evosim --config example_config.yaml stop")
    
    if success:
        print("✅ Evolution stopped successfully")
    else:
        print(f"❌ Failed to stop evolution: {error}")
    
    return True

def run_interactive_demo():
    """Run an interactive demonstration."""
    print("\n🎮 Starting interactive demonstration...")
    print("This will open the EvoSim CLI. Try these commands:")
    print("  start     - Start evolution")
    print("  status    - Show current status")
    print("  stats     - Show detailed statistics")
    print("  pause     - Pause evolution")
    print("  resume    - Resume evolution")
    print("  stop      - Stop evolution")
    print("  exit      - Exit the program")
    print("\nPress Enter to continue...")
    input()
    
    # Start interactive mode
    success, output, error = run_command(
        "./build/bin/evosim --config example_config.yaml --interactive",
        capture_output=False
    )
    
    if not success:
        print(f"❌ Interactive mode failed: {error}")
        return False
    
    return True

def analyze_results():
    """Analyze evolution results."""
    print("\n📈 Analyzing results...")
    
    # Check for saved states
    if os.path.exists("saves"):
        saves = [f for f in os.listdir("saves") if f.endswith(".evo")]
        if saves:
            print(f"✅ Found {len(saves)} saved states:")
            for save in sorted(saves)[-3:]:  # Show last 3
                print(f"  - {save}")
    
    # Check for log files
    if os.path.exists("evosim.log"):
        print("✅ Found log file: evosim.log")
        
        # Show last few lines
        try:
            with open("evosim.log", "r") as f:
                lines = f.readlines()
                if lines:
                    print("📝 Recent log entries:")
                    for line in lines[-5:]:
                        print(f"  {line.strip()}")
        except Exception as e:
            print(f"❌ Failed to read log: {e}")
    
    # The 'export' command is not yet implemented in the server.
    # This is a placeholder for future functionality.
    # success, output, error = run_command(
    #     "./build/bin/evosim --config example_config.yaml export --file results.json"
    # )
    
    if success:
        print("✅ Results exported to results.json")
        
        # Try to parse and show summary
        try:
            with open("results.json", "r") as f:
                data = json.load(f)
                if "statistics" in data:
                    stats = data["statistics"]
                    print("📊 Evolution Summary:")
                    print(f"  Generations: {stats.get('total_generations', 'N/A')}")
                    print(f"  Best Fitness: {stats.get('best_fitness', 'N/A')}")
                    print(f"  Population: {stats.get('current_population', 'N/A')}")
        except Exception as e:
            print(f"❌ Failed to parse results: {e}")
    else:
        print(f"❌ Failed to export results: {error}")

def cleanup():
    """Clean up temporary files."""
    print("\n🧹 Cleaning up...")
    
    files_to_remove = [
        "example_config.yaml",
        "results.json"
    ]
    
    for file in files_to_remove:
        if os.path.exists(file):
            os.remove(file)
            print(f"✅ Removed {file}")
    
    print("✅ Cleanup completed")

def main():
    """Main function."""
    print("=" * 60)
    print("EvoSim - Simple Evolution Example")
    print("=" * 60)
    
    # Check system
    try:
        if not check_system():
            sys.exit(1)
        
        create_config()
        
        if not run_simple_evolution():
            print("❌ Simple evolution failed")
            sys.exit(1)
            
        analyze_results()
    finally:
        cleanup()
    
    print("\n🎉 Example completed successfully!")
    print("For more information, see the README.md file.")

if __name__ == "__main__":
    main() 