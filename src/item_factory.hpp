#pragma once

#include "item.hpp"
#include "result.hpp"
#include "enums.hpp"
#include "logger.hpp"

#include <unordered_map>
#include <random>
#include <string>
#include <fstream>
#include <sstream>
#include <array>
#include <iterator>
#include <algorithm>
#include <cstddef>

class ItemFactory {
public:
    ItemFactory() {
        rng_.seed(std::random_device{}());
    }

    Result<Item> create(const std::string& id, int playerLevel = 1) {
        auto it = templates_.find(id);
        if (it == templates_.end())
            return Result<Item>::err("Unknown item id '" + id + "'");

        Item result = it->second; // copy the template
        result.levelReq = std::max(1, playerLevel - 2 + randInt(-1, 2));

        // apply rarity – higher rarity => higher stats
        result.rarity = randomRarity();
        float rarityMul = 1.0f + static_cast<float>(static_cast<int>(result.rarity)) * 0.2f;

        std::visit([&](auto& data) {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, WeaponData>) {
                data.damage = static_cast<int>(data.damage * rarityMul);
                if (data.durability > 0) data.durability = static_cast<int>(data.durability * rarityMul);
            } else if constexpr (std::is_same_v<T, ArmorData>) {
                data.defense = static_cast<int>(data.defense * rarityMul);
            } else if constexpr (std::is_same_v<T, ConsumableData>) {
                data.healAmount = static_cast<int>(data.healAmount * rarityMul);
            }
        }, result.data);

        std::string prefix = rarityPrefix(result.rarity);
        if (!prefix.empty())
            result.name = prefix + " " + result.name;

        if (result.type == ItemType::Material || result.type == ItemType::Consumable) {
            result.maxStack = 20;
        } else {
            result.maxStack = 1;
        }

        // add enchantments for equippable items based on rarity
        if (result.type == ItemType::Weapon || result.type == ItemType::Armor) {
            int count = enchantCountForRarity(result.rarity);
            addRandomEnchantments(result, count);
        }

        return Result<Item>::ok(std::move(result));
    }

    Result<Item> createRandomItem(int playerLevel = 1) {
        if (templates_.empty())
            return Result<Item>::err("No item templates loaded");

        std::uniform_int_distribution<std::size_t> dist(0, templates_.size() - 1);
        auto it = std::next(templates_.begin(), dist(rng_));
        return create(it->first, playerLevel);
    }

    Result<void> loadTemplates(const std::string& path) {
        std::ifstream in(path);
        if (!in) return Result<void>::err("Cannot open templates file '" + path + "'");
        std::string content((std::istreambuf_iterator<char>(in)), {});
        json j;
        try { j = json_parse(content); }
        catch (const std::exception& e) { return Result<void>::err("JSON parse error: " + std::string(e.what())); }

        if (!j.is_array())
            return Result<void>::err("Templates file must contain a JSON array");

        for (const auto& elem : j) {
            try {
                Item tmpl{};
                from_json(elem, tmpl);
                templates_[tmpl.id] = tmpl;
            } catch (const std::exception& e) {
                Log::warn("Failed to parse template: " + std::string(e.what()));
            }
        }

        Log::info("Loaded " + std::to_string(templates_.size()) + " item templates.");
        return Result<void>::ok();
    }

private:
    std::mt19937 rng_;
    std::unordered_map<std::string, Item> templates_;

    int randInt(int a, int b) { std::uniform_int_distribution<int> d(a, b); return d(rng_); }

    Rarity randomRarity() {
        std::array<std::pair<Rarity, int>, 5> table{{
            {Rarity::Common,    55},
            {Rarity::Uncommon,  25},
            {Rarity::Rare,      12},
            {Rarity::Epic,       6},
            {Rarity::Legendary,  2}
        }};
        int total = 0; for (auto& p : table) total += p.second;
        int roll = randInt(1, total);
        int acc = 0;
        for (auto& p : table) {
            acc += p.second;
            if (roll <= acc) return p.first;
        }
        return Rarity::Common;
    }

    std::string rarityPrefix(Rarity r) {
        switch (r) {
            case Rarity::Common:    return "";
            case Rarity::Uncommon:  return "Uncommon";
            case Rarity::Rare:      return "Rare";
            case Rarity::Epic:      return "Epic";
            case Rarity::Legendary: return "Legendary";
            default:                return "";
        }
    }

    // How many enchantments to roll for a given rarity
    int enchantCountForRarity(Rarity r) {
        switch (r) {
            case Rarity::Common:    return 0;
            case Rarity::Uncommon:  return (randInt(1, 100) <= 40) ? 1 : 0; // 40% chance
            case Rarity::Rare:      return 1;
            case Rarity::Epic:      return randInt(1, 2);
            case Rarity::Legendary: return randInt(2, 3);
            default:                return 0;
        }
    }

    void addRandomEnchantments(Item& item, int count) {
        static const std::array<Stat, 4> stats{
            Stat::Attack, Stat::Defense, Stat::Health, Stat::Mana};
        static const std::array<std::string, 5> elements{
            "", "Fire", "Ice", "Lightning", "Poison"};
        static const std::array<std::string, 8> suffixes{
            "of the Bear", "of the Fox", "of the Eagle",
            "of Power",    "of the Sage","of Warding",
            "of the Storm","of Fortune"};

        int rarityTier = static_cast<int>(item.rarity); // 0..4
        int baseMin = 1 + rarityTier;
        int baseMax = 3 + rarityTier * 2;

        for (int i = 0; i < count; ++i) {
            Enchantment e;
            e.stat    = stats[static_cast<std::size_t>(randInt(0, 3))];
            e.value   = randInt(baseMin, baseMax);
            e.element = elements[static_cast<std::size_t>(randInt(0, 4))];
            e.name    = suffixes[static_cast<std::size_t>(randInt(0, 7))];
            item.enchantments.push_back(std::move(e));
        }
    }
};
