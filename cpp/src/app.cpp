#include "app.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

void App::init() {
    settings = core::load_settings();
    versions = core::versions();
    if (versions.empty())
        versions.push_back(settings.version);
    if (std::find(versions.begin(), versions.end(), settings.version) ==
        versions.end())
        settings.version = "12.1";
    load_version();
}

void App::toast(int kind, const std::string& title,
                const std::string& body) {
    toasts.push_back(
        {next_toast_id++, kind, title, body,
         (float)SDL_GetTicks() / 1000.0f});
    while (toasts.size() > 4)
        toasts.pop_front();
}

void App::pump() {
    float now = (float)SDL_GetTicks() / 1000.0f;
    while (!toasts.empty() && now - toasts.front().at > TOAST_TTL + 0.5f)
        toasts.pop_front();
}

void App::load_version() {
    set = core::opcodes(settings.version);
    selected.clear();
    refilter();
    recheck();
}

void App::switch_version(const std::string& v) {
    if (v == settings.version)
        return;
    settings.version = v;
    core::save_settings(settings);
    load_version();
}

void App::refilter() {
    filtered.clear();
    ++filter_rev;
    if (!set)
        return;
    std::string q = search;
    for (auto& ch : q)
        ch = (char)tolower((unsigned char)ch);
    while (!q.empty() && isspace((unsigned char)q.front()))
        q.erase(q.begin());
    while (!q.empty() && isspace((unsigned char)q.back())
           )
        q.pop_back();
    for (auto& n : set->names) {
        if (q.empty()) {
            filtered.push_back(n);
            continue;
        }
        std::string low = n;
        for (auto& ch : low)
            ch = (char)tolower((unsigned char)ch);
        if (low.find(q) != std::string::npos)
            filtered.push_back(n);
    }
}

void App::flash() { flash_until = (float)SDL_GetTicks() / 1000.0f + 0.5f; }

/// Match a parsed value against the opcode list (mirrors the Svelte
/// checkOpcode): select + scroll-to + flash when found.
void App::check(uint64_t val) {
    if (!set || val > 0xFFFFFFFFull) {
        selected.clear();
        return;
    }
    auto it = set->value_to_name.find((uint32_t)val);
    if (it == set->value_to_name.end()) {
        selected.clear();
        return;
    }
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (filtered[i] == it->second) {
            selected = it->second;
            scroll_idx = (int)i;
            flash();
            return;
        }
    }
    selected.clear();
}

void App::recheck() {
    if (auto v = parse_dec(dec.c_str()))
        check(*v);
    else
        selected.clear();
}

std::optional<uint64_t> App::parse_hex(const char* s) {
    while (isspace((unsigned char)*s))
        ++s;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    if (!*s)
        return std::nullopt;
    char* end = nullptr;
    uint64_t v = strtoull(s, &end, 16);
    if (end == s || *end)
        return std::nullopt;
    return v;
}

std::optional<uint64_t> App::parse_dec(const char* s) {
    while (isspace((unsigned char)*s))
        ++s;
    if (!*s || *s == '-')
        return std::nullopt;
    char* end = nullptr;
    uint64_t v = strtoull(s, &end, 10);
    if (end == s)
        return std::nullopt;
    while (isspace((unsigned char)*end))
        ++end;
    if (*end)
        return std::nullopt;
    return v;
}

void App::on_hex_input() {
    auto v = parse_hex(hex.c_str());
    if (!v) {
        selected.clear();
        if (hex.empty())
            dec.clear();
        return;
    }
    char buf[32];
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)*v);
    dec = buf;
    check(*v);
}

void App::on_dec_input() {
    auto v = parse_dec(dec.c_str());
    if (!v) {
        selected.clear();
        if (dec.empty())
            hex.clear();
        return;
    }
    char buf[32];
    snprintf(buf, sizeof buf, "0x%llX", (unsigned long long)*v);
    hex = buf;
    check(*v);
}

void App::select_name(const std::string& name) {
    selected = name;
    if (set) {
        auto it = set->name_to_value.find(name);
        if (it != set->name_to_value.end()) {
            char buf[32];
            snprintf(buf, sizeof buf, "0x%llX",
                     (unsigned long long)it->second);
            hex = buf;
            snprintf(buf, sizeof buf, "%llu",
                     (unsigned long long)it->second);
            dec = buf;
        }
    }
    flash();
}

void App::clear_all() {
    hex.clear();
    dec.clear();
    selected.clear();
}
