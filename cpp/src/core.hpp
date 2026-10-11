#pragma once
// core — settings persistence (%LOCALAPPDATA%\com.hexdectc.app) and the
// opcode store parsed from the embedded opcodes.json RCDATA resource.
// Settings keys are camelCase to stay compatible with the old Tauri app.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace core {

struct Settings {
    std::string version = "12.1";
    bool always_on_top = false;
    bool tray_on_close = true;
};

Settings load_settings();
void save_settings(const Settings& s);

struct OpcodeSet {
    // first name wins on collisions (same rule as the old app)
    std::unordered_map<uint32_t, std::string> value_to_name;
    std::unordered_map<std::string, uint32_t> name_to_value;
    std::vector<std::string> names; // sorted
};

/// Parse the embedded opcodes.json once. Returns sorted version ids.
std::vector<std::string> versions();
const OpcodeSet* opcodes(const std::string& version);

} // namespace core
