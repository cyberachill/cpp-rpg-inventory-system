#pragma once

#include "json.hpp"
#include "result.hpp"
#include "logger.hpp"

#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <iterator>
#include <random>
#include <algorithm>
#include <cmath>

/*======================================================================
 *  5) Crafting – data‑driven recipes + mastery system
 *====================================================================*/

// ── Quality tiers ───────────────────────────────────────────────────
enum class CraftQuality { Normal, Good, Excellent, Masterwork };

inline std::string toString(CraftQuality q) {
    switch (q) {
        case CraftQuality::Good:       return "Good";
        case CraftQuality::Excellent:  return "Excellent";
        case CraftQuality::Masterwork: return "Masterwork";
        default:                       return "Normal";
    }
}
inline std::string qualityColor(CraftQuality q) {
    switch (q) {
        case CraftQuality::Good:       return "\x1B[32m"; // green
        case CraftQuality::Excellent:  return "\x1B[34m"; // blue
        case CraftQuality::Masterwork: return "\x1B[33m"; // gold
        default:                       return "\x1B[0m";
    }
}
// stat multiplier applied to crafted item on success
inline float qualityMultiplier(CraftQuality q) {
    switch (q) {
        case CraftQuality::Good:       return 1.10f;
        case CraftQuality::Excellent:  return 1.25f;
        case CraftQuality::Masterwork: return 1.50f;
        default:                       return 1.00f;
    }
}

// ── Recipe ──────────────────────────────────────────────────────────
struct Recipe {
    std::string resultId;
    int         resultCount{1};
    std::unordered_map<std::string, int> ingredients;

    // mastery fields
    std::string category{"general"};    // "blacksmithing", "alchemy", etc.
    int         levelReq{1};            // player level required
    int         masteryReq{0};          // crafting mastery level required (0 = anyone)
    float       baseSuccessChance{1.0f};// 0..1; mastery bonus adds 5% per level above req
};

inline void to_json(json& j, const Recipe& r) {
    json ingObj;
    for (auto& [k,v] : r.ingredients) ingObj[k] = v;
    j = json{
        {"resultId",          r.resultId},
        {"resultCount",       r.resultCount},
        {"ingredients",       ingObj},
        {"category",          r.category},
        {"levelReq",          r.levelReq},
        {"masteryReq",        r.masteryReq},
        {"baseSuccessChance", r.baseSuccessChance}
    };
}
inline void from_json(const json& j, Recipe& r) {
    r.resultId    = j.at("resultId").get<std::string>();
    r.resultCount = j.value("resultCount", 1);
    r.category    = j.value("category",    std::string{"general"});
    r.levelReq    = j.value("levelReq",    1);
    r.masteryReq  = j.value("masteryReq",  0);
    r.baseSuccessChance = j.value("baseSuccessChance", 1.0f);
    const json& ing = j.at("ingredients");
    if (!ing.is_object())
        throw std::runtime_error("ingredients must be an object");
    r.ingredients.clear();
    for (auto& [key, val] : ing.items())
        r.ingredients[key] = val.get<int>();
}

// ── CraftingMastery ─────────────────────────────────────────────────
struct MasteryLevel {
    int   level{0};
    int   xp{0};
    int   xpToNext{50};  // xp needed for next level
};

class CraftingMastery {
public:
    // Returns current mastery level for a category (0 if never used)
    int level(const std::string& cat) const {
        auto it = data_.find(cat);
        return it == data_.end() ? 0 : it->second.level;
    }

    // Award XP after a successful craft; returns true if leveled up
    bool awardXP(const std::string& cat, int xp = 10) {
        auto& m = data_[cat];
        m.xp += xp;
        bool leveled = false;
        while (m.xp >= m.xpToNext) {
            m.xp      -= m.xpToNext;
            m.level   += 1;
            m.xpToNext = 50 + m.level * 25;
            Log::info("Crafting mastery '" + cat + "' reached level " + std::to_string(m.level));
            leveled = true;
        }
        return leveled;
    }

    void print() const {
        if (data_.empty()) { std::cout << "  (no mastery data yet)\n"; return; }
        for (const auto& [cat, m] : data_) {
            std::cout << "  " << cat << ": Lv " << m.level
                      << " (" << m.xp << "/" << m.xpToNext << " XP)\n";
        }
    }

    // Effective success chance given recipe and mastery
    float successChance(const Recipe& rec) const {
        int ml  = level(rec.category);
        if (ml < rec.masteryReq) return 0.0f; // blocked
        float bonus = static_cast<float>(ml - rec.masteryReq) * 0.05f;
        return std::min(1.0f, rec.baseSuccessChance + bonus);
    }

    // Roll quality tier based on mastery advantage
    CraftQuality rollQuality(const Recipe& rec, std::mt19937& rng) const {
        int advantage = level(rec.category) - rec.masteryReq;
        if (advantage < 0) advantage = 0;

        // base thresholds (out of 100): Normal<45, Good<70, Excellent<90, Masterwork
        // each mastery level above req shifts thresholds up by 3 points
        int shift = std::min(advantage * 3, 30);
        int normalCap     = std::max(5,  45 - shift);
        int goodCap       = std::max(25, 70 - shift);
        int excellentCap  = std::max(60, 90 - shift);

        std::uniform_int_distribution<int> d(1, 100);
        int roll = d(rng);
        if (roll <= normalCap)    return CraftQuality::Normal;
        if (roll <= goodCap)      return CraftQuality::Good;
        if (roll <= excellentCap) return CraftQuality::Excellent;
        return CraftQuality::Masterwork;
    }

private:
    std::unordered_map<std::string, MasteryLevel> data_;
};

// ── CraftingSystem ───────────────────────────────────────────────────
class CraftingSystem {
public:
    Result<void> loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) return Result<void>::err("Cannot open recipe file '" + path + "'");
        std::string content((std::istreambuf_iterator<char>(in)), {});
        json j;
        try { j = json_parse(content); }
        catch (const std::exception& e) {
            return Result<void>::err("JSON parse error: " + std::string(e.what()));
        }
        if (!j.is_array())
            return Result<void>::err("Recipes file must contain a JSON array");

        for (const auto& elem : j) {
            try {
                Recipe rec{};
                from_json(elem, rec);
                recipes_[rec.resultId] = rec;
            } catch (const std::exception& e) {
                Log::warn("Failed to parse recipe: " + std::string(e.what()));
            }
        }
        Log::info("Loaded " + std::to_string(recipes_.size()) + " recipes.");
        return Result<void>::ok();
    }

    const Recipe* get(const std::string& resultId) const {
        auto it = recipes_.find(resultId);
        return it == recipes_.end() ? nullptr : &it->second;
    }

    bool has(const std::string& resultId) const { return recipes_.count(resultId) > 0; }

    void listRecipes() const {
        for (const auto& [id, rec] : recipes_) {
            std::cout << "  " << id << " [" << rec.category << "]"
                      << " lvReq:" << rec.levelReq
                      << " mastReq:" << rec.masteryReq;
            if (rec.baseSuccessChance < 1.0f)
                std::cout << " chance:" << static_cast<int>(rec.baseSuccessChance * 100) << "%";
            std::cout << "\n";
        }
    }

private:
    std::unordered_map<std::string, Recipe> recipes_;
};
