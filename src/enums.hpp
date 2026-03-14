#pragma once

#include <string>

/*======================================================================
 *  2) Core enums & helpers
 *====================================================================*/
enum class ItemType   { Weapon, Armor, Consumable, Material, Misc };
enum class EquipSlot  { Head, Chest, Legs, Feet, Hands, Weapon, Shield, Ring1, Ring2, Accessory, None };
enum class Rarity     { Common, Uncommon, Rare, Epic, Legendary };
enum class Stat       { Attack, Defense, Health, Mana, MaxHP, Speed, CritChance };

inline std::string toString(ItemType t) {
    switch (t) {
        case ItemType::Weapon:     return "Weapon";
        case ItemType::Armor:      return "Armor";
        case ItemType::Consumable: return "Consumable";
        case ItemType::Material:   return "Material";
        default:                   return "Misc";
    }
}
inline ItemType stringToItemType(const std::string& s) {
    if (s == "Weapon") return ItemType::Weapon;
    if (s == "Armor")  return ItemType::Armor;
    if (s == "Consumable") return ItemType::Consumable;
    if (s == "Material")   return ItemType::Material;
    return ItemType::Misc;
}
inline std::string toString(Rarity r) {
    switch (r) {
        case Rarity::Common:    return "Common";
        case Rarity::Uncommon:  return "Uncommon";
        case Rarity::Rare:      return "Rare";
        case Rarity::Epic:      return "Epic";
        case Rarity::Legendary: return "Legendary";
        default:                return "Unknown";
    }
}
inline Rarity stringToRarity(const std::string& s) {
    if (s == "Common")    return Rarity::Common;
    if (s == "Uncommon")  return Rarity::Uncommon;
    if (s == "Rare")      return Rarity::Rare;
    if (s == "Epic")      return Rarity::Epic;
    if (s == "Legendary") return Rarity::Legendary;
    return Rarity::Common;
}
inline std::string toString(EquipSlot s) {
    switch (s) {
        case EquipSlot::Head:       return "Head";
        case EquipSlot::Chest:      return "Chest";
        case EquipSlot::Legs:       return "Legs";
        case EquipSlot::Feet:       return "Feet";
        case EquipSlot::Hands:      return "Hands";
        case EquipSlot::Weapon:     return "Weapon";
        case EquipSlot::Shield:     return "Shield";
        case EquipSlot::Ring1:      return "Ring1";
        case EquipSlot::Ring2:      return "Ring2";
        case EquipSlot::Accessory:  return "Accessory";
        default:                    return "None";
    }
}
inline EquipSlot stringToEquipSlot(const std::string& s) {
    if (s == "Head")      return EquipSlot::Head;
    if (s == "Chest")     return EquipSlot::Chest;
    if (s == "Legs")      return EquipSlot::Legs;
    if (s == "Feet")      return EquipSlot::Feet;
    if (s == "Hands")     return EquipSlot::Hands;
    if (s == "Weapon")    return EquipSlot::Weapon;
    if (s == "Shield")    return EquipSlot::Shield;
    if (s == "Ring1")     return EquipSlot::Ring1;
    if (s == "Ring2")     return EquipSlot::Ring2;
    if (s == "Accessory") return EquipSlot::Accessory;
    return EquipSlot::None;
}
inline std::string toString(Stat s) {
    switch (s) {
        case Stat::Attack:     return "Attack";
        case Stat::Defense:    return "Defense";
        case Stat::Health:     return "Health";
        case Stat::Mana:       return "Mana";
        case Stat::MaxHP:      return "MaxHP";
        case Stat::Speed:      return "Speed";
        case Stat::CritChance: return "CritChance";
        default:               return "Unknown";
    }
}
inline Stat stringToStat(const std::string& s) {
    if (s == "Attack")     return Stat::Attack;
    if (s == "Defense")    return Stat::Defense;
    if (s == "Health")     return Stat::Health;
    if (s == "Mana")       return Stat::Mana;
    if (s == "MaxHP")      return Stat::MaxHP;
    if (s == "Speed")      return Stat::Speed;
    if (s == "CritChance") return Stat::CritChance;
    return Stat::Attack;
}
inline std::string rarityColor(Rarity r) {
    switch (r) {
        case Rarity::Common:    return "\x1B[37m";
        case Rarity::Uncommon:  return "\x1B[32m";
        case Rarity::Rare:      return "\x1B[34m";
        case Rarity::Epic:      return "\x1B[35m";
        case Rarity::Legendary: return "\x1B[33m";
        default:                return "\x1B[0m";
    }
}
inline std::string resetColor() { return "\x1B[0m"; }
