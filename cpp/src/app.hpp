#pragma once
// UI-agnostic app state: settings, opcode store, the filtered list,
// hex<->dec conversion, selection/scroll/flash requests and in-app
// toasts. The RmlUi layer in main.cpp only reads/writes this.

#include "core.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

constexpr float TOAST_TTL = 5.0f;

struct ToastMsg {
    uint32_t id;
    int kind; // 0 info, 1 ok, 2 err
    std::string title, body;
    float at; // seconds (SDL ticks)
};

struct App {
    core::Settings settings;
    std::vector<std::string> versions;
    const core::OpcodeSet* set = nullptr;

    // text-field mirrors (bound two-way to inputs)
    std::string search, hex, dec;

    std::string selected;                 // selected opcode name
    std::vector<std::string> filtered;    // names matching `search`
    uint32_t filter_rev = 0;              // bumped when `filtered` changes

    int scroll_idx = -1;                  // request: scroll filtered[i] into view
    float flash_until = -1.0f;            // list border pulse until this time

    std::deque<ToastMsg> toasts;
    uint32_t next_toast_id = 1;

    void init();
    void toast(int kind, const std::string& title, const std::string& body);
    /// Expire toasts; call once per frame.
    void pump();

    void switch_version(const std::string& v);
    void refilter();
    void on_hex_input();
    void on_dec_input();
    void select_name(const std::string& name);
    void clear_all();

private:
    void load_version();
    void flash();
    void check(uint64_t val);
    void recheck();
    static std::optional<uint64_t> parse_hex(const char* s);
    static std::optional<uint64_t> parse_dec(const char* s);
};
