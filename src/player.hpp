#pragma once

#include "enums.hpp"
#include "item.hpp"

#include <string>
#include <iostream>
#include <algorithm>
#include <unordered_map>

// Forward declarations — avoid circular includes
class Inventory;
class SetBonusManager;

/*======================================================================
 *  Computed stat totals (base + equipment + enchantments + set bonuses)
 *====================================================================*/
struct PlayerStats {
    int attack{0};
    int defense{0};
    int maxHP{0};
    int mana{0};
    int speed{0};
    int critChance{0};

    void add(Stat s, int v) {
        switch (s) {
            case Stat::Attack:     attack     += v; break;
            case Stat::Defense:    defense    += v; break;
            case Stat::Health:     maxHP      += v; break;  // alias
            case Stat::MaxHP:      maxHP      += v; break;
            case Stat::Mana:       mana       += v; break;
            case Stat::Speed:      speed      += v; break;
            case Stat::CritChance: critChance += v; break;
        }
    }
};

/*======================================================================
 *  Player — level, base stats, current resources
 *====================================================================*/
class Player {
public:
    explicit Player(int level = 1)
        : level_(level)
        , baseAttack_   (10 + level * 2)
        , baseDefense_  ( 5 + level)
        , baseMaxHP_    (80 + level * 20)
        , baseMana_     (50 + level * 10)
        , baseSpeed_    (10)
        , baseCritChance_(5)
        , currentHP_    (80 + level * 20)
        , currentMana_  (50 + level * 10)
    {}

    int level()       const { return level_; }
    int currentHP()   const { return currentHP_; }
    int currentMana() const { return currentMana_; }

    // Aggregate all stat sources: base + equipped items + enchantments + set bonuses
    PlayerStats computeStats(const Inventory& inv,
                             const SetBonusManager& setMgr) const;

    // Restore HP by amount; returns actual HP gained (capped at maxHP)
    int applyHeal(int amount, const PlayerStats& stats) {
        int before  = currentHP_;
        currentHP_  = std::min(stats.maxHP, currentHP_ + amount);
        return currentHP_ - before;
    }

    // Restore mana by amount (capped at stats.mana)
    int applyManaRestore(int amount, const PlayerStats& stats) {
        int before  = currentMana_;
        currentMana_ = std::min(stats.mana, currentMana_ + amount);
        return currentMana_ - before;
    }

    void takeDamage(int amount) {
        currentHP_ = std::max(0, currentHP_ - amount);
    }

    bool isAlive() const { return currentHP_ > 0; }

    void setLevel(int lvl) {
        level_         = lvl;
        baseAttack_    = 10 + lvl * 2;
        baseDefense_   =  5 + lvl;
        baseMaxHP_     = 80 + lvl * 20;
        baseMana_      = 50 + lvl * 10;
        currentHP_     = std::min(currentHP_, baseMaxHP_);
        currentMana_   = std::min(currentMana_, baseMana_);
    }

    void printSheet(const Inventory& inv, const SetBonusManager& setMgr) const;

private:
    int level_;
    int baseAttack_, baseDefense_, baseMaxHP_, baseMana_, baseSpeed_, baseCritChance_;
    int currentHP_, currentMana_;
};

// ── Out-of-class definitions (need full Inventory / SetBonusManager) ──
// These are in player_impl.hpp, included at the bottom of main.cpp
// to break circular dependency (inventory.hpp includes player.hpp indirectly).
// We provide them as inline definitions here after forward-declaring the
// necessary free functions used by Inventory/SetBonusManager.

#include "inventory.hpp"
#include "set_bonus.hpp"

inline PlayerStats Player::computeStats(const Inventory& inv,
                                         const SetBonusManager& setMgr) const {
    PlayerStats s;
    s.attack     = baseAttack_;
    s.defense    = baseDefense_;
    s.maxHP      = baseMaxHP_;
    s.mana       = baseMana_;
    s.speed      = baseSpeed_;
    s.critChance = baseCritChance_;

    // Equipment: weapon damage → attack, armor defense → defense
    const auto& equip = inv.getEquipment();
    for (const auto& [slot, ptr] : equip) {
        if (!ptr) continue;
        if (const auto* wd = std::get_if<WeaponData>(&ptr->data))
            s.attack += wd->damage;
        if (const auto* ad = std::get_if<ArmorData>(&ptr->data))
            s.defense += ad->defense;
        // Enchantment bonuses
        for (const auto& e : ptr->enchantments)
            s.add(e.stat, e.value);
    }

    // Set bonus enchantments
    for (const auto& [name, bonus] : setMgr.activeBonus(inv))
        for (const auto& e : bonus.bonuses)
            s.add(e.stat, e.value);

    return s;
}

inline void Player::printSheet(const Inventory& inv,
                                const SetBonusManager& setMgr) const {
    auto s = computeStats(inv, setMgr);
    std::cout << "\n\x1B[1m--- Character Sheet (Level " << level_ << ") ---\x1B[0m\n";
    std::cout << "  HP:         \x1B[31m" << currentHP_   << " / " << s.maxHP
              << "\x1B[0m";
    if (s.maxHP != baseMaxHP_)
        std::cout << "  (base " << baseMaxHP_ << ")";
    std::cout << "\n";
    std::cout << "  Mana:       \x1B[35m" << currentMana_ << " / " << s.mana
              << "\x1B[0m";
    if (s.mana != baseMana_)
        std::cout << "  (base " << baseMana_ << ")";
    std::cout << "\n";
    std::cout << "  Attack:     \x1B[31m" << s.attack    << "\x1B[0m";
    if (s.attack != baseAttack_)
        std::cout << "  (base " << baseAttack_ << ")";
    std::cout << "\n";
    std::cout << "  Defense:    \x1B[34m" << s.defense   << "\x1B[0m";
    if (s.defense != baseDefense_)
        std::cout << "  (base " << baseDefense_ << ")";
    std::cout << "\n";
    std::cout << "  Speed:      \x1B[36m" << s.speed     << "\x1B[0m\n";
    std::cout << "  Crit:       \x1B[33m" << s.critChance << "%\x1B[0m\n";
}
