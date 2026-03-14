#pragma once

#include "item.hpp"
#include "inventory.hpp"
#include "enchantment.hpp"
#include "json.hpp"
#include "logger.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>

/*======================================================================
 *  Set Bonus System
 *
 *  An ArmorSet groups items by a shared "setId" tag.
 *  As more pieces are worn, threshold bonuses activate.
 *
 *  Each SetBonus holds:
 *    - piecesRequired  : how many set items must be equipped
 *    - bonuses         : list of Enchantment-style stat buffs
 *    - description     : human-readable text
 *
 *  Items declare membership via the "setId" field in the JSON template
 *  (stored in MiscData::setId or a new top-level field on Item).
 *
 *  To keep Item layout minimal we store setId as a plain string on Item.
 *====================================================================*/

struct SetBonus {
    int                      piecesRequired{2};
    std::vector<Enchantment> bonuses;
    std::string              description;
};

struct ArmorSet {
    std::string            id;
    std::string            name;
    std::vector<SetBonus>  thresholds;  // sorted by piecesRequired ascending
};

// ---------- JSON helpers ----------

inline void from_json(const json& j, SetBonus& b) {
    b.piecesRequired = j.value("piecesRequired", 2);
    b.description    = j.value("description", std::string{});
    b.bonuses.clear();
    if (j.contains("bonuses") && j["bonuses"].is_array()) {
        for (const auto& je : j["bonuses"]) {
            Enchantment e{};
            from_json(je, e);
            b.bonuses.push_back(std::move(e));
        }
    }
}

inline void from_json(const json& j, ArmorSet& s) {
    s.id   = j.at("id").get<std::string>();
    s.name = j.at("name").get<std::string>();
    s.thresholds.clear();
    for (const auto& jt : j.at("thresholds")) {
        SetBonus b{};
        from_json(jt, b);
        s.thresholds.push_back(std::move(b));
    }
    // keep sorted
    std::sort(s.thresholds.begin(), s.thresholds.end(),
        [](const SetBonus& a, const SetBonus& b){ return a.piecesRequired < b.piecesRequired; });
}

// ---------- Manager ----------

class SetBonusManager {
public:
    Result<void> loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) return Result<void>::err("Cannot open set bonus file '" + path + "'");
        std::string content((std::istreambuf_iterator<char>(in)), {});
        json j;
        try { j = json_parse(content); }
        catch (const std::exception& e) {
            return Result<void>::err("JSON parse error: " + std::string(e.what()));
        }
        if (!j.is_array())
            return Result<void>::err("Set bonus file must be a JSON array");

        for (const auto& elem : j) {
            try {
                ArmorSet s{};
                from_json(elem, s);
                sets_[s.id] = std::move(s);
            } catch (const std::exception& e) {
                Log::warn("Failed to parse armor set: " + std::string(e.what()));
            }
        }
        Log::info("Loaded " + std::to_string(sets_.size()) + " armor sets.");
        return Result<void>::ok();
    }

    // Count how many pieces of each set are currently equipped
    std::unordered_map<std::string, int> countEquippedPieces(const Inventory& inv) const {
        std::unordered_map<std::string, int> counts;
        const auto& equip = inv.getEquipment();
        for (const auto& [slot, ptr] : equip) {
            if (ptr && !ptr->setId.empty())
                counts[ptr->setId]++;
        }
        return counts;
    }

    // Collect all active bonuses given the current equipment
    std::vector<std::pair<std::string /*setName*/, SetBonus>> activeBonus(
            const Inventory& inv) const
    {
        std::vector<std::pair<std::string, SetBonus>> active;
        auto counts = countEquippedPieces(inv);

        for (const auto& [sid, count] : counts) {
            auto it = sets_.find(sid);
            if (it == sets_.end()) continue;
            const ArmorSet& s = it->second;

            // collect all thresholds that are met
            for (const auto& threshold : s.thresholds) {
                if (count >= threshold.piecesRequired)
                    active.emplace_back(s.name, threshold);
            }
        }
        return active;
    }

    // Print active set bonuses for the player
    void printActiveBonuses(const Inventory& inv) const {
        auto active = activeBonus(inv);
        if (active.empty()) {
            std::cout << "  No set bonuses active.\n";
            return;
        }
        for (const auto& [name, bonus] : active) {
            std::cout << "  \x1B[33m[" << name << " — "
                      << bonus.piecesRequired << " pieces]\x1B[0m "
                      << bonus.description << "\n";
            std::cout << enchantmentLine(bonus.bonuses) << "\n";
        }
    }

    // Print all known sets and their thresholds
    void printAllSets() const {
        for (const auto& [sid, s] : sets_) {
            std::cout << "  \x1B[33m" << s.name << "\x1B[0m (id: " << sid << ")\n";
            for (const auto& t : s.thresholds) {
                std::cout << "    " << t.piecesRequired << " pcs: " << t.description
                          << enchantmentLine(t.bonuses) << "\n";
            }
        }
    }

    bool has(const std::string& id) const { return sets_.count(id) > 0; }

private:
    std::unordered_map<std::string, ArmorSet> sets_;
};
