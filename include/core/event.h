#pragma once

namespace evosim {
    
    /**
     * @brief Evolution event types
     */
    enum class EventType {
        GENERATION_STARTED,     ///< Generation started
        GENERATION_COMPLETED,   ///< Generation completed
        ORGANISM_BORN,          ///< New organism created
        ORGANISM_DIED,          ///< Organism died
        FITNESS_IMPROVED,       ///< Best fitness improved
        POPULATION_CHANGED,     ///< Population size changed
        ENGINE_STARTED,         ///< Engine started
        ENGINE_STOPPED,         ///< Engine stopped
        ENGINE_PAUSED,          ///< Engine paused
        ENGINE_RESUMED,         ///< Engine resumed
        STATE_SAVED,            ///< State saved
        STATE_LOADED,           ///< State loaded
        ERROR_OCCURRED          ///< Error occurred
    };

}