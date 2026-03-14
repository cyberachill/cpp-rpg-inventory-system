#pragma once

#include "item.hpp"
#include "item_factory.hpp"
#include "result.hpp"
#include "logger.hpp"
#include "json.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>

/*======================================================================
 *  LootTable — weighted, data-driven drop system
 *
 *  Each entry in a table specifies:
 *    - itemId   : template id passed to ItemFactory
 *    - weight   : relative chance (higher = more likely)
 *    - minQty / maxQty : stack size to award
 *    - guaranteed : always drops regardless of roll
 *
 *  Tables can be chained: one table may reference another table
 *  by prefixing the id with "table:" (e.g. "table:common_drops").
 *
 *  LootTableManager loads all tables from a JSON file and resolves
 *  them on demand via roll().
 *====================================================================*/

struct LootEntry {
    std::string id;          // item template id OR "table:<name>" for sub-tables
    int         weight{100};
    int         minQty{1};
    int         maxQty{1};
    bool        guaranteed{false};
};

struct LootTable {
    std::string            name;
    std::vector<LootEntry> entries;
    int                    rolls{1};  // how many times to sample the table
};

// ---------- JSON helpers ----------

inline void from_json(const json& j, LootEntry& e) {
    e.id        = j.at("id").get<std::string>();
    e.weight    = j.value("weight",    100);
    e.minQty    = j.value("minQty",      1);
    e.maxQty    = j.value("maxQty",      1);
    e.guaranteed= j.value("guaranteed", false);
}

inline void from_json(const json& j, LootTable& t) {
    t.name  = j.at("name").get<std::string>();
    t.rolls = j.value("rolls", 1);
    t.entries.clear();
    for (const auto& je : j.at("entries")) {
        LootEntry e{};
        from_json(je, e);
        t.entries.push_back(std::move(e));
    }
}

// ---------- Manager ----------

class LootTableManager {
public:
    Result<void> loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) return Result<void>::err("Cannot open loot table file '" + path + "'");
        std::string content((std::istreambuf_iterator<char>(in)), {});
        json j;
        try { j = json_parse(content); }
        catch (const std::exception& e) {
            return Result<void>::err("JSON parse error: " + std::string(e.what()));
        }
        if (!j.is_array())
            return Result<void>::err("Loot table file must be a JSON array");

        for (const auto& elem : j) {
            try {
                LootTable lt{};
                from_json(elem, lt);
                tables_[lt.name] = std::move(lt);
            } catch (const std::exception& e) {
                Log::warn("Failed to parse loot table: " + std::string(e.what()));
            }
        }
        Log::info("Loaded " + std::to_string(tables_.size()) + " loot tables.");
        return Result<void>::ok();
    }

    bool has(const std::string& name) const { return tables_.count(name) > 0; }

    // Roll a named table and return a list of (itemId, quantity) pairs.
    // Sub-table references are resolved recursively (max 4 levels deep).
    std::vector<std::pair<std::string, int>> roll(
            const std::string& tableName,
            std::mt19937&      rng,
            int                depth = 0) const
    {
        std::vector<std::pair<std::string, int>> drops;
        if (depth > 4) return drops; // guard against infinite loops

        auto it = tables_.find(tableName);
        if (it == tables_.end()) {
            Log::warn("Loot table '" + tableName + "' not found");
            return drops;
        }
        const LootTable& table = it->second;

        // Separate guaranteed and weighted entries
        std::vector<const LootEntry*> weighted;
        int totalWeight = 0;
        for (const auto& e : table.entries) {
            if (e.guaranteed) {
                addDrop(drops, e, rng, depth);
            } else {
                weighted.push_back(&e);
                totalWeight += e.weight;
            }
        }

        // Perform `rolls` weighted samples
        for (int r = 0; r < table.rolls && totalWeight > 0; ++r) {
            std::uniform_int_distribution<int> dist(1, totalWeight);
            int roll = dist(rng);
            int acc  = 0;
            for (const auto* e : weighted) {
                acc += e->weight;
                if (roll <= acc) {
                    addDrop(drops, *e, rng, depth);
                    break;
                }
            }
        }
        return drops;
    }

    // Convenience: roll table and create items via ItemFactory.
    // Returns list of created items.
    std::vector<Item> rollItems(
            const std::string& tableName,
            ItemFactory&       factory,
            std::mt19937&      rng,
            int                playerLevel = 1) const
    {
        std::vector<Item> items;
        for (auto& [id, qty] : roll(tableName, rng)) {
            for (int q = 0; q < qty; ++q) {
                auto res = factory.create(id, playerLevel);
                if (res) items.push_back(res.value());
                else     Log::warn("LootTable: factory failed for '" + id + "': " + res.error());
            }
        }
        return items;
    }

    const std::unordered_map<std::string, LootTable>& getTables() const { return tables_; }

private:
    std::unordered_map<std::string, LootTable> tables_;

    void addDrop(std::vector<std::pair<std::string, int>>& drops,
                 const LootEntry& e,
                 std::mt19937& rng,
                 int depth) const
    {
        if (e.id.rfind("table:", 0) == 0) {
            // recurse into sub-table
            std::string sub = e.id.substr(6);
            auto sub_drops = roll(sub, rng, depth + 1);
            for (auto& d : sub_drops) drops.push_back(std::move(d));
        } else {
            int qty = e.minQty;
            if (e.maxQty > e.minQty) {
                std::uniform_int_distribution<int> qd(e.minQty, e.maxQty);
                qty = qd(rng);
            }
            drops.emplace_back(e.id, qty);
        }
    }
};
