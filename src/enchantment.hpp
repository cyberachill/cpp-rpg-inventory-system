#pragma once

#include "json.hpp"
#include "enums.hpp"

#include <string>
#include <vector>

/*======================================================================
 *  Enchantment — a stat bonus attached to an item
 *====================================================================*/
struct Enchantment {
    Stat        stat{Stat::Attack};
    int         value{0};       // bonus magnitude
    std::string element;        // "Fire", "Ice", "Lightning", "Poison", "" = none
    std::string name;           // display name, e.g. "of the Bear"
};

inline void to_json(json& j, const Enchantment& e) {
    j = json{
        {"stat",    toString(e.stat)},
        {"value",   e.value},
        {"element", e.element},
        {"name",    e.name}
    };
}
inline void from_json(const json& j, Enchantment& e) {
    e.stat    = stringToStat(j.at("stat").get<std::string>());
    e.value   = j.at("value").get<int>();
    e.element = j.value("element", std::string{});
    e.name    = j.value("name",    std::string{});
}

// ANSI colour per stat for display
inline std::string statColor(Stat s) {
    switch (s) {
        case Stat::Attack:     return "\x1B[31m"; // red
        case Stat::Defense:    return "\x1B[34m"; // blue
        case Stat::Health:     return "\x1B[32m"; // green
        case Stat::Mana:       return "\x1B[35m"; // purple
        case Stat::MaxHP:      return "\x1B[32m"; // green
        case Stat::Speed:      return "\x1B[36m"; // cyan
        case Stat::CritChance: return "\x1B[33m"; // yellow
        default:               return "\x1B[0m";
    }
}

// Build a short summary line for getDescription
inline std::string enchantmentLine(const std::vector<Enchantment>& encs) {
    if (encs.empty()) return {};
    std::string out;
    for (const auto& e : encs) {
        out += " " + statColor(e.stat) + "[+" + std::to_string(e.value) + " " + toString(e.stat);
        if (!e.element.empty()) out += " (" + e.element + ")";
        if (!e.name.empty())    out += " " + e.name;
        out += "]" + resetColor();
    }
    return out;
}
