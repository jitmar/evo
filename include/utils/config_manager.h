#pragma once

#include <string>
#include <map>
#include <variant>
#include <memory>
#include <fstream>
#include <sstream>
#include <functional>
#include <vector>

namespace evosim {

/**
 * @brief Configuration value types
 */
using ConfigValue = std::variant<bool, int, double, std::string>;

/**
 * @brief Configuration manager for the evolution system
 * 
 * Provides a flexible configuration system with support for
 * multiple file formats, validation, and default values.
 */
class ConfigManager {
public:
    /**
     * @brief Configuration section
     */
    using ConfigSection = std::map<std::string, ConfigValue>;

    /**
     * @brief Configuration data
     */
    using ConfigData = std::map<std::string, ConfigSection>;

    /**
     * @brief Configuration validation rule
     */
    struct ValidationRule {
        std::string key;                 ///< Configuration key
        std::string type;                ///< Expected type
        bool required = false;           ///< Whether required
        ConfigValue default_value;       ///< Default value
        std::string description;         ///< Description
        std::function<bool(const ConfigValue&)> validator; ///< Custom validator
    };

    /**
     * @brief Configuration schema
     */
    using ConfigSchema = std::vector<ValidationRule>;

    /**
     * @brief Constructor
     */
    ConfigManager();

    /**
     * @brief Destructor
     */
    ~ConfigManager() = default;

    /**
     * @brief Load configuration from file
     * @param filename Configuration file path
     * @return True if loaded successfully
     */
    bool loadFromFile(const std::string& filename);

    /**
     * @brief Save configuration to file
     * @param filename Configuration file path
     * @return True if saved successfully
     */
    bool saveToFile(const std::string& filename) const;

    /**
     * @brief Load configuration from string
     * @param content Configuration content
     * @param format File format ("ini", "json", "yaml")
     * @return True if loaded successfully
     */
    bool loadFromString(const std::string& content, const std::string& format = "ini");

    /**
     * @brief Save configuration to string
     * @param format File format ("ini", "json", "yaml")
     * @return Configuration string
     */
    std::string saveToString(const std::string& format = "ini") const;

    /**
     * @brief Get configuration value
     * @param section Configuration section
     * @param key Configuration key
     * @param default_value Default value if not found
     * @return Configuration value
     */
    template<typename T>
    T get(const std::string& section, const std::string& key, const T& default_value = T{}) const {
        auto value = getValue(section, key);
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        return default_value;
    }

    /**
     * @brief Set configuration value
     * @param section Configuration section
     * @param key Configuration key
     * @param value Configuration value
     */
    template<typename T>
    void set(const std::string& section, const std::string& key, const T& value) {
        setValue(section, key, ConfigValue(value));
    }

    /**
     * @brief Check if configuration key exists
     * @param section Configuration section
     * @param key Configuration key
     * @return True if exists
     */
    bool has(const std::string& section, const std::string& key) const;

    /**
     * @brief Remove configuration key
     * @param section Configuration section
     * @param key Configuration key
     * @return True if removed
     */
    bool remove(const std::string& section, const std::string& key);

    /**
     * @brief Get configuration section
     * @param section Section name
     * @return Configuration section
     */
    ConfigSection getSection(const std::string& section) const;

    /**
     * @brief Set configuration section
     * @param section Section name
     * @param config_section Configuration section
     */
    void setSection(const std::string& section, const ConfigSection& config_section);

    /**
     * @brief Remove configuration section
     * @param section Section name
     * @return True if removed
     */
    bool removeSection(const std::string& section);

    /**
     * @brief Get all configuration data
     * @return Configuration data
     */
    const ConfigData& getData() const { return data_; }

    /**
     * @brief Set all configuration data
     * @param data Configuration data
     */
    void setData(const ConfigData& data) { data_ = data; }

    /**
     * @brief Clear all configuration data
     */
    void clear() { data_.clear(); }

    /**
     * @brief Merge configuration from another manager
     * @param other Other configuration manager
     * @param overwrite Whether to overwrite existing values
     */
    void merge(const ConfigManager& other, bool overwrite = true);

    /**
     * @brief Validate configuration against schema
     * @param schema Configuration schema
     * @return Vector of validation errors
     */
    std::vector<std::string> validate(const ConfigSchema& schema) const;

    /**
     * @brief Apply default values from schema
     * @param schema Configuration schema
     */
    void applyDefaults(const ConfigSchema& schema);

    /**
     * @brief Get configuration as string
     * @param section Configuration section
     * @param key Configuration key
     * @param default_value Default value
     * @return String value
     */
    std::string getString(const std::string& section, const std::string& key, 
                         const std::string& default_value = "") const;

    /**
     * @brief Get configuration as integer
     * @param section Configuration section
     * @param key Configuration key
     * @param default_value Default value
     * @return Integer value
     */
    int getInt(const std::string& section, const std::string& key, int default_value = 0) const;

    /**
     * @brief Get configuration as double
     * @param section Configuration section
     * @param key Configuration key
     * @param default_value Default value
     * @return Double value
     */
    double getDouble(const std::string& section, const std::string& key, double default_value = 0.0) const;

    /**
     * @brief Get configuration as boolean
     * @param section Configuration section
     * @param key Configuration key
     * @param default_value Default value
     * @return Boolean value
     */
    bool getBool(const std::string& section, const std::string& key, bool default_value = false) const;

    /**
     * @brief Get configuration keys in section
     * @param section Configuration section
     * @return Vector of keys
     */
    std::vector<std::string> getKeys(const std::string& section) const;

    /**
     * @brief Get configuration sections
     * @return Vector of section names
     */
    std::vector<std::string> getSections() const;

    /**
     * @brief Check if section exists
     * @param section Section name
     * @return True if exists
     */
    bool hasSection(const std::string& section) const;

    /**
     * @brief Get configuration value with type checking
     * @param section Configuration section
     * @param key Configuration key
     * @return Configuration value
     */
    ConfigValue getValue(const std::string& section, const std::string& key) const;

    /**
     * @brief Set configuration value
     * @param section Configuration section
     * @param key Configuration key
     * @param value Configuration value
     */
    void setValue(const std::string& section, const std::string& key, const ConfigValue& value);

    /**
     * @brief Convert value to string
     * @param value Configuration value
     * @return String representation
     */
    static std::string valueToString(const ConfigValue& value);

    /**
     * @brief Convert string to value
     * @param str String value
     * @param type Target type
     * @return Configuration value
     */
    static ConfigValue stringToValue(const std::string& str, const std::string& type);

private:
    ConfigData data_;                    ///< Configuration data

    /**
     * @brief Parse INI format
     * @param content INI content
     * @return True if parsed successfully
     */
    bool parseINI(const std::string& content);

    /**
     * @brief Parse JSON format
     * @param content JSON content
     * @return True if parsed successfully
     */
    bool parseJSON(const std::string& content);

    /**
     * @brief Parse YAML format
     * @param content YAML content
     * @return True if parsed successfully
     */
    bool parseYAML(const std::string& content);

    /**
     * @brief Generate INI format
     * @return INI string
     */
    std::string generateINI() const;

    /**
     * @brief Generate JSON format
     * @return JSON string
     */
    std::string generateJSON() const;

    /**
     * @brief Generate YAML format
     * @return YAML string
     */
    std::string generateYAML() const;

    /**
     * @brief Trim string
     * @param str String to trim
     * @return Trimmed string
     */
    static std::string trim(const std::string& str);

    /**
     * @brief Split string
     * @param str String to split
     * @param delimiter Delimiter
     * @return Vector of substrings
     */
    static std::vector<std::string> split(const std::string& str, char delimiter);

    /**
     * @brief Escape string
     * @param str String to escape
     * @return Escaped string
     */
    static std::string escape(const std::string& str);

    /**
     * @brief Unescape string
     * @param str String to unescape
     * @return Unescaped string
     */
    static std::string unescape(const std::string& str);
};

} // namespace evosim 