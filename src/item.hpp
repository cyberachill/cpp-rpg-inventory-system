#pragma once

#include "json.hpp"
#include "enums.hpp"
#include "result.hpp"

#include <variant>
#include <string>
#include <sstream>
#include <cassert>

/*======================================================================
 *  3) Item data structures (type‑erased) + JSON conversions
 *====================================================================*/
struct WeaponData {
    int damage{0};
    int durability{-1};           // -1 = no durability (e.g. magical sword)
    int weight{0};
};
struct ArmorData {
    int defense{0};
    int weight{0};
};
struct ConsumableData {
    int healAmount{0};
    int weight{0};
};
struct MaterialData {
    int weight{0};
};
struct MiscData {
    int weight{0};
};

inline void to_json(json& j, const WeaponData& w){
    j = json{{"damage",w.damage},{"durability",w.durability},{"weight",w.weight}};
}
inline void from_json(const json& j, WeaponData& w){
    w.damage = j.value("damage",0);
    w.durability = j.value("durability",-1);
    w.weight = j.value("weight",0);
}

inline void to_json(json& j, const ArmorData& a){
    j = json{{"defense",a.defense},{"weight",a.weight}};
}
inline void from_json(const json& j, ArmorData& a){
    a.defense = j.value("defense",0);
    a.weight = j.value("weight",0);
}

inline void to_json(json& j, const ConsumableData& c){
    j = json{{"healAmount",c.healAmount},{"weight",c.weight}};
}
inline void from_json(const json& j, ConsumableData& c){
    c.healAmount = j.value("healAmount",0);
    c.weight = j.value("weight",0);
}

inline void to_json(json& j, const MaterialData& m){
    j = json{{"weight",m.weight}};
}
inline void from_json(const json& j, MaterialData& m){
    m.weight = j.value("weight",0);
}

inline void to_json(json& j, const MiscData& m){
    j = json{{"weight",m.weight}};
}
inline void from_json(const json& j, MiscData& m){
    m.weight = j.value("weight",0);
}

using ItemPayload = std::variant<
    WeaponData,
    ArmorData,
    ConsumableData,
    MaterialData,
    MiscData
>;

struct Item {
    std::string id;               // e.g. "iron_sword"
    std::string name;             // human readable
    ItemType    type{ItemType::Misc};
    Rarity     rarity{Rarity::Common};
    int        levelReq{1};
    int        stackSize{1};      // how many we have in this stack
    int        maxStack{1};       // max per slot (1 = non‑stackable)
    ItemPayload data;             // type‑specific fields

    [[nodiscard]] int getWeight() const {
        struct Visitor {
            int stackSize;
            int operator()(const WeaponData& w)      const { return w.weight * stackSize; }
            int operator()(const ArmorData& a)       const { return a.weight * stackSize; }
            int operator()(const ConsumableData& c)  const { return c.weight * stackSize; }
            int operator()(const MaterialData& m)    const { return m.weight * stackSize; }
            int operator()(const MiscData& m)        const { return m.weight * stackSize; }
        };
        return std::visit(Visitor{stackSize}, data);
    }

    [[nodiscard]] int weightPerUnit() const {
        assert(stackSize > 0 && "stackSize must be > 0");
        return getWeight() / stackSize;
    }

    std::string getDescription() const {
        std::ostringstream ss;
        ss << rarityColor(rarity) << name << resetColor()
           << " (x" << stackSize << ") – " << toString(type);
        std::visit([&](auto&& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, WeaponData>) {
                ss << " [DMG:" << d.damage
                   << (d.durability >= 0 ? ("/" + std::to_string(d.durability)) : "")
                   << "]";
            } else if constexpr (std::is_same_v<T, ArmorData>) {
                ss << " [DEF:" << d.defense << "]";
            } else if constexpr (std::is_same_v<T, ConsumableData>) {
                ss << " [HEAL:" << d.healAmount << "]";
            }
        }, data);
        return ss.str();
    }

    std::string serialize() const {
        json j;
        j = *this;          // use assignment operator which invokes to_json
        return j.dump();
    }

    static Result<Item> deserialize(const std::string& jsonStr) {
        try {
            json j = json::parse(jsonStr);
            Item it = j.get<Item>();
            return Result<Item>::ok(std::move(it));
        } catch (const std::exception& e) {
            return Result<Item>::err(e.what());
        }
    }
};

/* -----------------------------------------------------------------
   JSON conversion for Item (required by our json class)
   ----------------------------------------------------------------- */
inline void to_json(json& j, const Item& i){
    j = json{
        {"id", i.id},
        {"name", i.name},
        {"type", toString(i.type)},
        {"rarity", toString(i.rarity)},
        {"levelReq", i.levelReq},
        {"stackSize", i.stackSize},
        {"maxStack", i.maxStack}
    };
    std::visit([&j](auto&& d){ j["data"] = d; }, i.data);
}
inline void from_json(const json& j, Item& i){
    i.id        = j.at("id").get<std::string>();
    i.name      = j.at("name").get<std::string>();
    i.type      = stringToItemType(j.at("type").get<std::string>());
    i.rarity    = stringToRarity(j.at("rarity").get<std::string>());
    i.levelReq  = j.at("levelReq").get<int>();
    i.stackSize = j.at("stackSize").get<int>();
    i.maxStack  = j.at("maxStack").get<int>();
    const json& d = j.at("data");
    switch (i.type) {
        case ItemType::Weapon:      i.data = d.get<WeaponData>();      break;
        case ItemType::Armor:       i.data = d.get<ArmorData>();       break;
        case ItemType::Consumable:  i.data = d.get<ConsumableData>();  break;
        case ItemType::Material:    i.data = d.get<MaterialData>();    break;
        default:                    i.data = d.get<MiscData>();        break;
    }
}
