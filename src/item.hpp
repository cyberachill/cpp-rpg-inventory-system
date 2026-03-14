#pragma once

#include "json.hpp"
#include "enums.hpp"
#include "result.hpp"

#include <variant>
#include <string>
#include <sstream>
#include <cassert>
#include <algorithm>

/*======================================================================
 *  3) Item data structures (type‑erased) + JSON conversions
 *====================================================================*/
struct WeaponData {
    int damage{0};
    int durability{-1};     // current; -1 = indestructible
    int maxDurability{-1};  // -1 = indestructible
    int weight{0};
};
struct ArmorData {
    int defense{0};
    int durability{80};
    int maxDurability{80};
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
    int repairAmount{0};  // > 0 → acts as a repair kit
    int weight{0};
};

inline void to_json(json& j, const WeaponData& w){
    j = json{{"damage",w.damage},{"durability",w.durability},{"maxDurability",w.maxDurability},{"weight",w.weight}};
}
inline void from_json(const json& j, WeaponData& w){
    w.damage        = j.value("damage",0);
    w.durability    = j.value("durability",-1);
    w.maxDurability = j.value("maxDurability", w.durability);
    w.weight        = j.value("weight",0);
}

inline void to_json(json& j, const ArmorData& a){
    j = json{{"defense",a.defense},{"durability",a.durability},{"maxDurability",a.maxDurability},{"weight",a.weight}};
}
inline void from_json(const json& j, ArmorData& a){
    a.defense       = j.value("defense",0);
    a.durability    = j.value("durability",80);
    a.maxDurability = j.value("maxDurability", a.durability);
    a.weight        = j.value("weight",0);
}

inline void to_json(json& j, const ConsumableData& c){
    j = json{{"healAmount",c.healAmount},{"weight",c.weight}};
}
inline void from_json(const json& j, ConsumableData& c){
    c.healAmount = j.value("healAmount",0);
    c.weight     = j.value("weight",0);
}

inline void to_json(json& j, const MaterialData& m){
    j = json{{"weight",m.weight}};
}
inline void from_json(const json& j, MaterialData& m){
    m.weight = j.value("weight",0);
}

inline void to_json(json& j, const MiscData& m){
    j = json{{"repairAmount",m.repairAmount},{"weight",m.weight}};
}
inline void from_json(const json& j, MiscData& m){
    m.repairAmount = j.value("repairAmount",0);
    m.weight       = j.value("weight",0);
}

using ItemPayload = std::variant<
    WeaponData,
    ArmorData,
    ConsumableData,
    MaterialData,
    MiscData
>;

struct Item {
    std::string id;
    std::string name;
    ItemType    type{ItemType::Misc};
    Rarity     rarity{Rarity::Common};
    int        levelReq{1};
    int        stackSize{1};
    int        maxStack{1};
    ItemPayload data;

    [[nodiscard]] int getWeight() const {
        struct Visitor {
            int stackSize;
            int operator()(const WeaponData& w)     const { return w.weight * stackSize; }
            int operator()(const ArmorData& a)      const { return a.weight * stackSize; }
            int operator()(const ConsumableData& c) const { return c.weight * stackSize; }
            int operator()(const MaterialData& m)   const { return m.weight * stackSize; }
            int operator()(const MiscData& m)       const { return m.weight * stackSize; }
        };
        return std::visit(Visitor{stackSize}, data);
    }

    [[nodiscard]] int weightPerUnit() const {
        assert(stackSize > 0 && "stackSize must be > 0");
        return getWeight() / stackSize;
    }

    // Returns true if this item has durability and it has reached zero
    [[nodiscard]] bool isBroken() const {
        return std::visit([](auto&& d) -> bool {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, WeaponData>)
                return d.durability == 0;
            if constexpr (std::is_same_v<T, ArmorData>)
                return d.durability == 0;
            return false;
        }, data);
    }

    // Reduces durability by amount. Returns true if changed, false if indestructible.
    bool degrade(int amount = 1) {
        return std::visit([amount](auto&& d) -> bool {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, WeaponData>) {
                if (d.durability < 0) return false;
                d.durability = std::max(0, d.durability - amount);
                return true;
            }
            if constexpr (std::is_same_v<T, ArmorData>) {
                if (d.durability < 0) return false;
                d.durability = std::max(0, d.durability - amount);
                return true;
            }
            return false;
        }, data);
    }

    // Restores durability by amount, capped at maxDurability.
    // Returns true if repair occurred, false if already full or indestructible.
    bool repair(int amount) {
        return std::visit([amount](auto&& d) -> bool {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, WeaponData>) {
                if (d.maxDurability < 0 || d.durability >= d.maxDurability) return false;
                d.durability = std::min(d.maxDurability, d.durability + amount);
                return true;
            }
            if constexpr (std::is_same_v<T, ArmorData>) {
                if (d.maxDurability < 0 || d.durability >= d.maxDurability) return false;
                d.durability = std::min(d.maxDurability, d.durability + amount);
                return true;
            }
            return false;
        }, data);
    }

    std::string getDescription() const {
        std::ostringstream ss;
        ss << rarityColor(rarity) << name << resetColor()
           << " (x" << stackSize << ") – " << toString(type);
        std::visit([&](auto&& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, WeaponData>) {
                ss << " [DMG:" << d.damage;
                if (d.maxDurability >= 0)
                    ss << " DUR:" << d.durability << "/" << d.maxDurability;
                ss << "]";
                if (d.durability == 0)
                    ss << " \x1B[31m[BROKEN]\x1B[0m";
            } else if constexpr (std::is_same_v<T, ArmorData>) {
                ss << " [DEF:" << d.defense;
                if (d.maxDurability >= 0)
                    ss << " DUR:" << d.durability << "/" << d.maxDurability;
                ss << "]";
                if (d.durability == 0)
                    ss << " \x1B[31m[BROKEN]\x1B[0m";
            } else if constexpr (std::is_same_v<T, ConsumableData>) {
                ss << " [HEAL:" << d.healAmount << "]";
            } else if constexpr (std::is_same_v<T, MiscData>) {
                if (d.repairAmount > 0)
                    ss << " [REPAIR:" << d.repairAmount << "]";
            }
        }, data);
        return ss.str();
    }

    // Defined after to_json/from_json below (to_json/from_json must be visible)
    std::string serialize() const;
    static Result<Item> deserialize(const std::string& jsonStr);
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

// Out-of-class definitions — to_json/from_json must be declared above
inline std::string Item::serialize() const {
    json j;
    to_json(j, *this);
    return j.dump();
}

inline Result<Item> Item::deserialize(const std::string& jsonStr) {
    try {
        json j = json_parse(jsonStr);
        Item it{};
        from_json(j, it);
        return Result<Item>::ok(std::move(it));
    } catch (const std::exception& e) {
        return Result<Item>::err(e.what());
    }
}
