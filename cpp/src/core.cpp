#include "core.hpp"

#include <nlohmann/json.hpp>

#include <Windows.h>
#include <shlobj.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using nlohmann::json;

namespace core {

// ------------------------------------------------------------ settings

static fs::path settings_path() {
    wchar_t* p = nullptr;
    fs::path dir = ".";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr,
                                     &p))) {
        dir = fs::path(p) / "com.hexdectc.app";
        CoTaskMemFree(p);
    }
    return dir / "settings.json";
}

Settings load_settings() {
    Settings s;
    std::ifstream f(settings_path());
    if (!f)
        return s;
    try {
        json j = json::parse(f);
        s.version = j.value("version", s.version);
        s.always_on_top = j.value("alwaysOnTop", s.always_on_top);
        s.tray_on_close = j.value("trayOnClose", s.tray_on_close);
    } catch (...) {
    }
    return s;
}

void save_settings(const Settings& s) {
    fs::path p = settings_path();
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    json j = {{"version", s.version},
              {"alwaysOnTop", s.always_on_top},
              {"trayOnClose", s.tray_on_close}};
    std::ofstream(p) << j.dump(2);
}

// ------------------------------------------------------------ opcodes

static const char* embedded_opcodes(size_t* size) {
    HRSRC r =
        FindResourceW(nullptr, L"OPCODES_JSON", MAKEINTRESOURCEW(10));
    if (!r)
        return nullptr;
    HGLOBAL g = LoadResource(nullptr, r);
    if (!g)
        return nullptr;
    *size = SizeofResource(nullptr, r);
    return (const char*)LockResource(g);
}

static std::unordered_map<std::string, OpcodeSet> g_sets;
static std::vector<std::string> g_versions;
static bool g_loaded = false;

static void load() {
    if (g_loaded)
        return;
    g_loaded = true;
    size_t size = 0;
    const char* data = embedded_opcodes(&size);
    if (!data || !size)
        return;
    json root;
    try {
        root = json::parse(data, data + size);
    } catch (...) {
        return;
    }
    for (auto& [ver, entries] : root.items()) {
        OpcodeSet set;
        for (auto& [key, name] : entries.items()) {
            if (!key.starts_with("0x"))
                continue; // decimal twin keys are for the old frontend
            uint32_t v = 0;
            try {
                v = (uint32_t)std::stoul(key.substr(2), nullptr, 16);
            } catch (...) {
                continue;
            }
            if (!set.value_to_name.count(v))
                set.value_to_name.emplace(v, name.get<std::string>());
            if (!set.name_to_value.count(name.get<std::string>()))
                set.name_to_value.emplace(name.get<std::string>(), v);
        }
        set.names.reserve(set.name_to_value.size());
        for (auto& [n, _] : set.name_to_value)
            set.names.push_back(n);
        std::sort(set.names.begin(), set.names.end());
        g_sets.emplace(ver, std::move(set));
        g_versions.push_back(ver);
    }
    std::sort(g_versions.begin(), g_versions.end());
}

std::vector<std::string> versions() {
    load();
    return g_versions;
}

const OpcodeSet* opcodes(const std::string& version) {
    load();
    auto it = g_sets.find(version);
    return it == g_sets.end() ? nullptr : &it->second;
}

} // namespace core
