#pragma once

#include "json.hpp"
#include "result.hpp"
#include "logger.hpp"

#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <iterator>

/*======================================================================
 *  5) Crafting – data‑driven recipes (JSON)
 *====================================================================*/
struct Recipe {
    std::string resultId;                               // what we produce
    int         resultCount{1};                         // how many we get
    std::unordered_map<std::string, int> ingredients;  // id → quantity
};

inline void to_json(json& j, const Recipe& r){
    j = json{{"resultId", r.resultId},
             {"resultCount", r.resultCount},
             {"ingredients", json{} } };
    json ingObj;
    for (auto& kv : r.ingredients) ingObj[kv.first] = kv.second;
    j["ingredients"] = ingObj;
}
inline void from_json(const json& j, Recipe& r){
    r.resultId = j.at("resultId").get<std::string>();
    r.resultCount = j.value("resultCount",1);
    const json& ing = j.at("ingredients");
    if (!ing.is_object())
        throw std::runtime_error("ingredients must be an object");
    r.ingredients.clear();
    for (auto it = ing.object_begin(); it != ing.object_end(); ++it) {
        r.ingredients[it.key()] = it.value().get<int>();
    }
}

class CraftingSystem {
public:
    Result<void> loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) return Result<void>::err("Cannot open recipe file '" + path + "'");
        std::string content((std::istreambuf_iterator<char>(in)), {});
        json j;
        try { j = json::parse(content); }
        catch (const std::exception& e) { return Result<void>::err("JSON parse error: " + std::string(e.what())); }

        if (!j.is_array())
            return Result<void>::err("Recipes file must contain a JSON array");

        for (const auto& elem : j) {
            try {
                Recipe rec = elem.get<Recipe>();
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

private:
    std::unordered_map<std::string, Recipe> recipes_;
};
