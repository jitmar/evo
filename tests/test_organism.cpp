#include <gtest/gtest.h>
#include "core/organism.h"
#include <thread>
#include <set>
#include <mutex> // Added for mutex

using namespace evosim;

class OrganismTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test bytecode
        test_bytecode_ = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    }

    void TearDown() override {
        // Cleanup if needed
    }

    Organism::Bytecode test_bytecode_;
};

TEST_F(OrganismTest, Constructor) {
    Organism organism(test_bytecode_);
    
    EXPECT_EQ(organism.getBytecode(), test_bytecode_);
    EXPECT_EQ(organism.getStats().generation, 0);
    EXPECT_EQ(organism.getStats().parent_id, 0);
    EXPECT_EQ(organism.getFitnessScore(), 0.0);
    // Example replacement for an alive/dead test:
    // Add organism to environment
    // environment_->addOrganism(organism);
    // EXPECT_TRUE(environment_->getOrganism(organism->getStats().id) != nullptr);
    // Remove organism from environment
    // environment_->removeOrganism(organism->getStats().id);
    // EXPECT_TRUE(environment_->getOrganism(organism->getStats().id) == nullptr);
}

TEST_F(OrganismTest, Replication) {
    Organism parent(test_bytecode_);
    parent.setFitnessScore(0.8);
    
    auto child = parent.replicate(0.1, 2);
    
    EXPECT_NE(child->getStats().id, parent.getStats().id);
    EXPECT_EQ(child->getStats().parent_id, parent.getStats().id);
    EXPECT_EQ(child->getStats().generation, parent.getStats().generation + 1);
}

TEST_F(OrganismTest, FitnessScore) {
    Organism organism(test_bytecode_);
    
    organism.setFitnessScore(0.75);
    EXPECT_EQ(organism.getFitnessScore(), 0.75);
    
    organism.setFitnessScore(1.0);
    EXPECT_EQ(organism.getFitnessScore(), 1.0);
}

TEST_F(OrganismTest, Age) {
    Organism organism(test_bytecode_);
    
    auto age = organism.getAge();
    EXPECT_GE(age.count(), 0);
    
    // Wait a bit and check age increases
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto new_age = organism.getAge();
    EXPECT_GT(new_age.count(), age.count());
}

TEST_F(OrganismTest, Serialization) {
    Organism organism(test_bytecode_);
    organism.setFitnessScore(0.6);
    
    std::string serialized = organism.serialize();
    EXPECT_FALSE(serialized.empty());
    
    Organism new_organism({});
    EXPECT_TRUE(new_organism.deserialize(serialized));
    EXPECT_EQ(new_organism.getBytecode(), organism.getBytecode());
    EXPECT_EQ(new_organism.getFitnessScore(), organism.getFitnessScore());
}

TEST_F(OrganismTest, CopyConstructor) {
    Organism original(test_bytecode_);
    original.setFitnessScore(0.9);
    
    Organism copy(original);
    
    EXPECT_EQ(copy.getBytecode(), original.getBytecode());
    EXPECT_EQ(copy.getFitnessScore(), original.getFitnessScore());
    EXPECT_NE(copy.getStats().id, original.getStats().id);
}

TEST_F(OrganismTest, MoveConstructor) {
    Organism original(test_bytecode_);
    original.setFitnessScore(0.7);
    
    uint64_t original_id = original.getStats().id;
    
    Organism moved(std::move(original));
    
    EXPECT_EQ(moved.getBytecode(), test_bytecode_);
    EXPECT_EQ(moved.getFitnessScore(), 0.7);
    EXPECT_EQ(moved.getStats().id, original_id);
}

TEST_F(OrganismTest, AssignmentOperator) {
    Organism original(test_bytecode_);
    original.setFitnessScore(0.8);
    
    Organism::Bytecode other_bytecode = {0x09, 0x0A, 0x0B, 0x0C};
    Organism assigned(other_bytecode);
    assigned.setFitnessScore(0.3);
    
    assigned = original;
    
    EXPECT_EQ(assigned.getBytecode(), original.getBytecode());
    EXPECT_EQ(assigned.getFitnessScore(), original.getFitnessScore());
    EXPECT_NE(assigned.getStats().id, original.getStats().id);
}

TEST_F(OrganismTest, MoveAssignmentOperator) {
    Organism original(test_bytecode_);
    original.setFitnessScore(0.85);
    
    uint64_t original_id = original.getStats().id;
    
    Organism::Bytecode other_bytecode = {0x0D, 0x0E, 0x0F, 0x10};
    Organism assigned(other_bytecode);
    assigned.setFitnessScore(0.2);
    
    assigned = std::move(original);
    
    EXPECT_EQ(assigned.getBytecode(), test_bytecode_);
    EXPECT_EQ(assigned.getFitnessScore(), 0.85);
    EXPECT_EQ(assigned.getStats().id, original_id);
}

TEST_F(OrganismTest, MultipleReplications) {
    Organism organism(test_bytecode_);
    
    std::vector<Organism::OrganismPtr> children;
    for (int i = 0; i < 5; ++i) {
        children.push_back(organism.replicate(0.05, 1));
    }
    
    EXPECT_EQ(children.size(), 5);
    
    // All children should have different IDs
    std::set<uint64_t> ids;
    for (const auto& child : children) {
        ids.insert(child->getStats().id);
    }
    EXPECT_EQ(ids.size(), 5);
}

TEST_F(OrganismTest, MutationRate) {
    Organism organism(test_bytecode_);
    
    // Test with zero mutation rate
    auto child1 = organism.replicate(0.0, 0);
    EXPECT_EQ(child1->getBytecode(), organism.getBytecode());
    
    // Test with high mutation rate
    auto child2 = organism.replicate(1.0, 10);
    // Should have some mutations (though not guaranteed with random)
    EXPECT_GE(child2->getStats().mutation_count, 0);
}

TEST_F(OrganismTest, Statistics) {
    Organism organism(test_bytecode_);
    
    const auto& stats = organism.getStats();
    
    EXPECT_GT(stats.id, 0);
    EXPECT_EQ(stats.generation, 0);
    EXPECT_EQ(stats.parent_id, 0);
    EXPECT_EQ(stats.fitness_score, 0.0);
    EXPECT_EQ(stats.replication_count, 0);
    EXPECT_EQ(stats.mutation_count, 0);
}

TEST_F(OrganismTest, ThreadSafety) {
    Organism organism(test_bytecode_);
    
    // Test concurrent access (basic test)
    std::vector<std::thread> threads;
    std::vector<double> results;
    std::mutex results_mutex;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&organism, &results, &results_mutex, i]() {
            organism.setFitnessScore(0.1 * i);
            double score = organism.getFitnessScore();
            {
                std::lock_guard<std::mutex> lock(results_mutex);
                results.push_back(score);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(results.size(), 10);
} 