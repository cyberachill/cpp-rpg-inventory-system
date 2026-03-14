#pragma once

#include "player.hpp"
#include "inventory.hpp"
#include "set_bonus.hpp"
#include "loot_table.hpp"
#include "item_factory.hpp"
#include "logger.hpp"

#include <string>
#include <vector>
#include <random>
#include <iostream>
#include <algorithm>

/*======================================================================
 *  Enemy — data + basic AI
 *====================================================================*/
struct Enemy {
    std::string name;
    int         hp{20};
    int         maxHP{20};
    int         attack{5};
    int         defense{2};
    int         goldReward{10};
    std::string lootTable;    // optional loot table name on death

    bool isAlive() const { return hp > 0; }

    // Returns damage dealt to the enemy (after defence reduction)
    int takeDamage(int rawDmg) {
        int dmg = std::max(1, rawDmg - defense);
        hp = std::max(0, hp - dmg);
        return dmg;
    }
};

/*======================================================================
 *  Enemy catalogue — predefined encounter types
 *====================================================================*/
inline std::vector<Enemy> enemyCatalogue() {
    auto make = [](std::string n, int hp, int atk, int def, int gold, std::string lt) {
        Enemy e; e.name=std::move(n); e.hp=hp; e.maxHP=hp;
        e.attack=atk; e.defense=def; e.goldReward=gold; e.lootTable=std::move(lt);
        return e;
    };
    return {
        make("Goblin",        25,  8,  1,  12, "goblin_loot"),
        make("Bandit",        40, 12,  3,  25, "bandit_loot"),
        make("Dungeon Guard", 60, 16,  6,  40, "dungeon_chest"),
        make("Dragon",       150, 28, 12, 150, "dragon_hoard"),
        make("Skeleton",      30, 10,  2,  15, "goblin_loot"),
        make("Dark Wizard",   45, 20,  1,  50, "dungeon_chest"),
    };
}

/*======================================================================
 *  CombatResult — outcome after a full encounter
 *====================================================================*/
struct CombatResult {
    bool  playerWon{false};
    bool  playerFled{false};
    int   goldGained{0};
    std::vector<Item> lootDropped;
};

/*======================================================================
 *  Combat engine — one full encounter
 *====================================================================*/
class CombatEngine {
public:
    CombatResult runEncounter(Enemy&          enemy,
                               Player&         player,
                               Inventory&      inv,
                               const SetBonusManager& setMgr,
                               LootTableManager& lootMgr,
                               ItemFactory&    factory,
                               std::mt19937&   rng)
    {
        CombatResult result;

        std::cout << "\n\x1B[1m=== ENCOUNTER: " << enemy.name << " ===\x1B[0m\n";
        std::cout << "  Enemy HP: " << enemy.hp << "  ATK: " << enemy.attack
                  << "  DEF: " << enemy.defense << "\n";

        while (player.isAlive() && enemy.isAlive()) {
            printCombatStatus(player, enemy, inv, setMgr);

            // ---- player action ----
            std::cout << "\n  Action: [a]ttack  [u]se item  [f]lee  > ";
            std::string action;
            std::getline(std::cin, action);
            if (action.empty()) action = "a";

            if (action[0] == 'f') {
                // 40% flee chance
                std::uniform_int_distribution<int> fleeDist(1, 100);
                if (fleeDist(rng) <= 40) {
                    std::cout << "  You managed to flee!\n";
                    result.playerFled = true;
                    return result;
                }
                std::cout << "  \x1B[31mCouldn't flee!\x1B[0m\n";
            } else if (action[0] == 'u') {
                std::cout << "  Consumable id: ";
                std::string cid;
                std::getline(std::cin, cid);
                auto useRes = inv.useConsumable(cid);
                if (!useRes) {
                    std::cout << "  \x1B[31m" << useRes.error() << "\x1B[0m\n";
                } else {
                    auto stats = player.computeStats(inv, setMgr);
                    int gained = player.applyHeal(useRes.value(), stats);
                    std::cout << "  Restored \x1B[32m" << gained << " HP\x1B[0m. "
                              << "HP: " << player.currentHP() << "/" << stats.maxHP << "\n";
                }
            } else {
                // attack
                auto stats = player.computeStats(inv, setMgr);
                std::uniform_int_distribution<int> dmgRoll(
                    static_cast<int>(stats.attack * 0.8f),
                    static_cast<int>(stats.attack * 1.2f));
                int rawDmg = std::max(1, dmgRoll(rng));

                std::uniform_int_distribution<int> critRoll(1, 100);
                bool crit = critRoll(rng) <= stats.critChance;
                if (crit) rawDmg = static_cast<int>(rawDmg * 1.5f);

                int dealt = enemy.takeDamage(rawDmg);
                std::cout << "  You attack";
                if (crit) std::cout << " \x1B[33m[CRIT!]\x1B[0m";
                std::cout << " — \x1B[31m" << dealt << " damage\x1B[0m to "
                          << enemy.name << ". (HP: " << enemy.hp << "/"
                          << enemy.maxHP << ")\n";

                // weapon degrades on attack
                inv.degradeEquipped(EquipSlot::Weapon, 2);
                const Item* w = inv.getEquipped(EquipSlot::Weapon);
                if (w && w->isBroken())
                    std::cout << "  \x1B[31mYour weapon has broken!\x1B[0m\n";
            }

            if (!enemy.isAlive()) break;

            // ---- enemy turn ----
            auto stats = player.computeStats(inv, setMgr);
            std::uniform_int_distribution<int> edRoll(
                static_cast<int>(enemy.attack * 0.8f),
                static_cast<int>(enemy.attack * 1.2f));
            int enemyRaw = std::max(1, edRoll(rng));
            int blocked  = std::max(0, stats.defense / 3);   // 1/3 of defense absorbs
            int received = std::max(1, enemyRaw - blocked);
            player.takeDamage(received);

            std::cout << "  \x1B[31m" << enemy.name << " attacks for "
                      << received << " damage\x1B[0m"
                      << " (blocked " << blocked << ")."
                      << " Your HP: " << player.currentHP()
                      << "/" << stats.maxHP << "\n";
        }

        if (!player.isAlive()) {
            std::cout << "\n\x1B[31mYou have been defeated by " << enemy.name << "!\x1B[0m\n";
            std::cout << "You wake up with 1 HP at the nearest town.\n";
            // respawn with 1 HP instead of game over
            // (access base HP via a small workaround — call applyHeal with big number after setting hp to 1)
            player.takeDamage(player.currentHP() - 1);  // leave at 1 HP conceptually
            return result;
        }

        // Victory
        result.playerWon = true;
        result.goldGained = enemy.goldReward;
        std::cout << "\n\x1B[32mVictory!\x1B[0m " << enemy.name
                  << " defeated. You earn \x1B[33m" << enemy.goldReward
                  << "g\x1B[0m.\n";

        // Roll loot
        if (!enemy.lootTable.empty() && lootMgr.has(enemy.lootTable)) {
            result.lootDropped = lootMgr.rollItems(
                enemy.lootTable, factory, rng, player.level());
        }

        Log::info("Combat victory vs " + enemy.name);
        return result;
    }

private:
    void printCombatStatus(const Player& player, const Enemy& enemy,
                           const Inventory& inv,
                           const SetBonusManager& setMgr) const {
        auto stats = player.computeStats(inv, setMgr);
        int hpPct   = stats.maxHP > 0 ? (player.currentHP() * 20 / stats.maxHP) : 0;
        int eHpPct  = enemy.maxHP > 0  ? (enemy.hp * 20 / enemy.maxHP)           : 0;

        std::string pBar(hpPct,  '#');
        pBar += std::string(20 - hpPct, '-');
        std::string eBar(eHpPct, '#');
        eBar += std::string(20 - eHpPct, '-');

        std::cout << "\n  You:        \x1B[32m[" << pBar << "]\x1B[0m "
                  << player.currentHP() << "/" << stats.maxHP << " HP\n";
        std::cout << "  " << enemy.name << ": \x1B[31m[" << eBar << "]\x1B[0m "
                  << enemy.hp << "/" << enemy.maxHP << " HP\n";
    }
};
