#include "inventory.hpp"
#include "item_factory.hpp"
#include "crafting.hpp"
#include "shop.hpp"
#include "loot_table.hpp"
#include "set_bonus.hpp"
#include "player.hpp"
#include "logger.hpp"

#include <iostream>
#include <fstream>
#include <limits>
#include <string>
#include <random>

int main() {
    Log::setFile("game.log");               // isteğe bağlı dosya logu
    ItemFactory      factory;
    CraftingSystem   crafting;
    CraftingMastery  mastery;
    Shop             shop;
    LootTableManager lootMgr;
    SetBonusManager  setMgr;
    std::mt19937     rng(std::random_device{}());
    int              playerGold = 200;

    if (auto r = factory.loadTemplates("templates.json"); !r) {
        Log::error("Cannot continue without item templates: " + r.error());
        return 1;
    }
    if (auto r = crafting.loadFromFile("recipes.json"); !r) {
        Log::error("Cannot continue without recipes: " + r.error());
        return 1;
    }
    // loot tables are optional — warn but continue if missing
    if (auto r = lootMgr.loadFromFile("loot_tables.json"); !r)
        Log::warn("Loot tables not loaded: " + r.error());
    // set bonuses are optional — warn but continue if missing
    if (auto r = setMgr.loadFromFile("set_bonuses.json"); !r)
        Log::warn("Set bonuses not loaded: " + r.error());

    Inventory inv(30, 300);                  // 30 slot, 300 ağırlık limiti
    Player    player(5);                     // level 5

    // Pre-seed basic recipes every crafter knows from the start
    for (const std::string& r : {
        "iron_ingot", "steel_ingot", "gold_ingot",
        "health_potion", "repair_kit_basic",
        "enchanting_stone", "iron_sword"
    }) inv.learnRecipeById(r);

    shop.stockFromFactory(factory, player.level());

    while (true) {
        std::cout << "\n--- MENU ---------------------------------------------------\n";
        auto curStats = player.computeStats(inv, setMgr);
        std::cout << "1) Show inventory  [Gold: " << playerGold << "g | HP: "
                  << player.currentHP() << "/" << curStats.maxHP << " | ATK: "
                  << curStats.attack << " | DEF: " << curStats.defense << "]\n";
        std::cout << "2) Show equipment\n";
        std::cout << "3) Add random loot\n";
        std::cout << "4) Craft item\n";
        std::cout << "5) Equip item\n";
        std::cout << "6) Unequip slot\n";
        std::cout << "7) Save game\n";
        std::cout << "8) Load game\n";
        std::cout << "9) Strike with weapon (degrades it)\n";
        std::cout << "10) Use repair kit\n";
        std::cout << "11) Enchant item\n";
        std::cout << "12) Visit shop (browse / buy)\n";
        std::cout << "13) Sell item\n";
        std::cout << "14) Restock shop\n";
        std::cout << "15) Open loot table (goblin/bandit/dungeon_chest/dragon_hoard)\n";
        std::cout << "16) Show crafting mastery\n";
        std::cout << "17) List all recipes\n";
        std::cout << "18) Show active set bonuses\n";
        std::cout << "19) List all armor sets\n";
        std::cout << "20) Character sheet\n";
        std::cout << "21) Use consumable\n";
        std::cout << "22) Sort inventory (name/type/rarity/weight)\n";
        std::cout << "23) Filter inventory\n";
        std::cout << "24) Split stack\n";
        std::cout << "25) Compare item to equipped\n";
        std::cout << "26) Disenchant item\n";
        std::cout << "27) Upgrade item (costs 3x upgrade_stone)\n";
        std::cout << "28) Use recipe scroll\n";
        std::cout << "29) Show known recipes\n";
        std::cout << "0) Exit\n";
        std::cout << "Choice: ";
        int choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 0) break;

        switch (choice) {
            case 1: {   // envanter göster
                const auto& items = inv.getItems();
                std::cout << "\n--- Inventory (slots used: " << inv.usedSlots()
                          << " / 30, weight: " << inv.totalWeight()
                          << " / 300) ---\n";
                for (size_t i = 0; i < items.size(); ++i) {
                    std::cout << i + 1 << ") " << items[i].getDescription() << "\n";
                }
                break;
            }
            case 2: {   // ekipmanı göster
                const auto& equip = inv.getEquipment();
                std::cout << "\n--- Equipment ------------------------------------------------\n";
                for (const auto& [slot, ptr] : equip) {
                    std::cout << toString(slot) << ": ";
                    if (ptr) std::cout << ptr->getDescription() << "\n";
                    else     std::cout << "(empty)\n";
                }
                break;
            }
            case 3: {   // rastgele ganimet ekle
                auto res = factory.createRandomItem(player.level());
                if (!res) {
                    std::cout << "Factory error: " << res.error() << "\n";
                    break;
                }
                auto addRes = inv.addItem(res.value());
                if (!addRes) std::cout << "Cannot add loot: " << addRes.error() << "\n";
                else           std::cout << "You found: " << res.value().getDescription() << "\n";
                break;
            }
            case 4: {   // öğe üret
                std::cout << "Enter recipe result id (e.g. iron_sword): ";
                std::string rid;
                std::getline(std::cin, rid);
                auto cr = inv.craft(rid, factory, crafting, mastery, rng, player.level());
                if (!cr.success) {
                    std::cout << "Craft failed: " << cr.errorMsg << "\n";
                } else {
                    std::cout << qualityColor(cr.quality) << "[" << toString(cr.quality) << "]"
                              << resetColor() << " Crafted successfully!\n";
                    if (cr.leveledUp) std::cout << "  Crafting mastery leveled up!\n";
                }
                break;
            }
            case 5: {   // ekipana tak
                std::cout << "Enter inventory item id to equip: ";
                std::string iid;
                std::getline(std::cin, iid);
                auto eqRes = inv.equip(iid, player.level());
                if (!eqRes) std::cout << "Equip failed: " << eqRes.error() << "\n";
                else        std::cout << "Equipped successfully.\n";
                break;
            }
            case 6: {   // ekipmanı çıkar
                std::cout << "Enter slot name (Head/Chest/Legs/Feet/Hands/Weapon/Shield/Ring1/Ring2/Accessory): ";
                std::string slotStr;
                std::getline(std::cin, slotStr);
                EquipSlot slot = stringToEquipSlot(slotStr);
                if (slot == EquipSlot::None) {
                    std::cout << "Invalid slot.\n";
                    break;
                }
                auto unRes = inv.unequip(slot);
                if (!unRes) std::cout << "Unequip failed: " << unRes.error() << "\n";
                else        std::cout << "Unequipped successfully.\n";
                break;
            }
            case 7: {   // kaydet
                std::ofstream out("savegame.json");
                if (!out) { std::cout << "Cannot open save file.\n"; break; }
                // wrap inventory JSON inside a top-level game state object
                json invJson = json_parse(inv.serialize());
                json save;
                save["version"] = 1;
                save["gold"]    = playerGold;
                save["level"]   = player.level();
                save["inventory"] = invJson;
                out << save.dump(4);
                std::cout << "Game saved to savegame.json\n";
                break;
            }
            case 8: {   // yükle
                std::ifstream in("savegame.json");
                if (!in) { std::cout << "Cannot open save file.\n"; break; }
                std::string content((std::istreambuf_iterator<char>(in)), {});
                try {
                    json save = json_parse(content);
                    // legacy format: top-level has "items" directly
                    bool legacy = save.contains("items");
                    const json& invJson = legacy ? save : save.at("inventory");
                    auto loadRes = inv.deserialize(invJson.dump());
                    if (!loadRes) { std::cout << "Load failed: " << loadRes.error() << "\n"; break; }
                    if (!legacy) {
                        playerGold = save.value("gold",  playerGold);
                        int lvl    = save.value("level", player.level());
                        player.setLevel(lvl);
                    }
                    std::cout << "Game loaded. [Gold: " << playerGold
                              << "g | Level: " << player.level() << "]\n";
                } catch (const std::exception& e) {
                    std::cout << "Load failed: " << e.what() << "\n";
                }
                break;
            }
            case 9: {   // darbe vur – silah yıpransın
                auto res = inv.degradeEquipped(EquipSlot::Weapon, 5);
                if (!res) {
                    std::cout << "Strike failed: " << res.error() << "\n";
                } else {
                    const Item* w = inv.getEquipped(EquipSlot::Weapon);
                    // compute actual damage: weapon base + enchantment bonuses + set bonuses
                    auto stats = player.computeStats(inv, setMgr);
                    // base roll: ±20% variance around total attack
                    std::uniform_int_distribution<int> dmgRoll(
                        static_cast<int>(stats.attack * 0.8f),
                        static_cast<int>(stats.attack * 1.2f));
                    int dmg = std::max(1, dmgRoll(rng));
                    // critical hit?
                    std::uniform_int_distribution<int> critRoll(1, 100);
                    bool crit = critRoll(rng) <= stats.critChance;
                    if (crit) dmg = static_cast<int>(dmg * 1.5f);

                    std::cout << "You swing your weapon";
                    if (crit) std::cout << " \x1B[33m[CRITICAL HIT!]\x1B[0m";
                    std::cout << " — dealing \x1B[31m" << dmg << " damage\x1B[0m!\n";
                    if (w) {
                        std::cout << "  " << w->getDescription() << "\n";
                        if (w->isBroken())
                            std::cout << "  \x1B[31mYour weapon has broken!\x1B[0m\n";
                    }
                }
                break;
            }
            case 11: {  // enchant
                std::cout << "Enter enchanting stone id (e.g. enchanting_stone): ";
                std::string stoneId;
                std::getline(std::cin, stoneId);
                std::cout << "Enter item id to enchant: ";
                std::string targetId;
                std::getline(std::cin, targetId);
                auto res = inv.enchantItem(targetId, stoneId, rng);
                if (!res) std::cout << "Enchant failed: " << res.error() << "\n";
                else      std::cout << "Enchantment applied!\n";
                break;
            }
            case 10: {  // tamir kiti kullan
                std::cout << "Enter repair kit id (e.g. repair_kit_basic): ";
                std::string kitId;
                std::getline(std::cin, kitId);
                std::cout << "Enter item id to repair: ";
                std::string targetId;
                std::getline(std::cin, targetId);
                auto res = inv.useRepairKit(kitId, targetId);
                if (!res) std::cout << "Repair failed: " << res.error() << "\n";
                else      std::cout << "Item repaired successfully.\n";
                break;
            }
            case 12: {  // shop browse / buy
                std::cout << "\n--- SHOP (Gold: " << playerGold << "g) ---\n";
                shop.printStock();
                std::cout << "Enter item number to buy (0 to cancel): ";
                int idx;
                std::cin >> idx;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (idx > 0) {
                    auto res = shop.buy(static_cast<std::size_t>(idx - 1), inv, playerGold);
                    if (!res) std::cout << "Buy failed: " << res.error() << "\n";
                    else      std::cout << "Purchased! Gold remaining: " << playerGold << "g\n";
                }
                break;
            }
            case 13: {  // sell
                std::cout << "Enter item id to sell: ";
                std::string sid;
                std::getline(std::cin, sid);
                // preview price first
                const auto& items = inv.getItems();
                auto pit = std::find_if(items.begin(), items.end(),
                    [&](const Item& i){ return i.id == sid; });
                if (pit != items.end()) {
                    std::cout << "Sell price: " << shop.sellPrice(*pit) << "g — confirm? (y/n): ";
                    char c; std::cin >> c;
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    if (c == 'y' || c == 'Y') {
                        auto res = shop.sell(sid, inv, playerGold);
                        if (!res) std::cout << "Sell failed: " << res.error() << "\n";
                        else      std::cout << "Sold! Gold: " << playerGold << "g\n";
                    }
                } else {
                    std::cout << "Item not found in inventory.\n";
                }
                break;
            }
            case 15: {  // loot table roll
                std::cout << "Enter loot table name (goblin_loot / bandit_loot / dungeon_chest / dragon_hoard): ";
                std::string tname;
                std::getline(std::cin, tname);
                if (!lootMgr.has(tname)) {
                    std::cout << "Unknown loot table '" << tname << "'\n";
                    break;
                }
                auto dropped = lootMgr.rollItems(tname, factory, rng, player.level());
                if (dropped.empty()) {
                    std::cout << "Nothing dropped.\n";
                } else {
                    std::cout << "You found:\n";
                    for (auto& item : dropped) {
                        auto addRes = inv.addItem(item);
                        if (!addRes)
                            std::cout << "  (dropped on floor — no space) " << item.getDescription() << "\n";
                        else
                            std::cout << "  + " << item.getDescription() << "\n";
                    }
                }
                break;
            }
            case 14: {  // restock
                shop.stockFromFactory(factory, player.level());
                std::cout << "Shop restocked with new items.\n";
                break;
            }
            case 16: {  // show mastery
                std::cout << "\n--- Crafting Mastery ---\n";
                mastery.print();
                break;
            }
            case 17: {  // list recipes
                std::cout << "\n--- Recipes ---\n";
                crafting.listRecipes();
                break;
            }
            case 18: {  // active set bonuses
                std::cout << "\n--- Active Set Bonuses ---\n";
                setMgr.printActiveBonuses(inv);
                break;
            }
            case 19: {  // list all sets
                std::cout << "\n--- All Armor Sets ---\n";
                setMgr.printAllSets();
                break;
            }
            case 20: {  // character sheet
                player.printSheet(inv, setMgr);
                break;
            }
            case 22: {  // sort inventory
                std::cout << "Sort by (name/type/rarity/weight): ";
                std::string sk;
                std::getline(std::cin, sk);
                Inventory::SortKey key = Inventory::SortKey::Type;
                if (sk == "name")   key = Inventory::SortKey::Name;
                else if (sk == "rarity") key = Inventory::SortKey::Rarity;
                else if (sk == "weight") key = Inventory::SortKey::Weight;
                inv.sort(key);
                std::cout << "Inventory sorted by " << sk << ".\n";
                break;
            }
            case 23: {  // filter inventory
                std::cout << "Search query: ";
                std::string query;
                std::getline(std::cin, query);
                auto results = inv.filter(query);
                if (results.empty()) {
                    std::cout << "No items match '" << query << "'.\n";
                } else {
                    std::cout << "  Found " << results.size() << " item(s):\n";
                    for (const auto* it : results)
                        std::cout << "  " << it->getDescription() << "\n";
                }
                break;
            }
            case 24: {  // split stack
                std::cout << "Enter slot number to split (1-based): ";
                int slotNum;
                std::cin >> slotNum;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter amount to split off: ";
                int splitAmt;
                std::cin >> splitAmt;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (slotNum < 1) { std::cout << "Invalid slot.\n"; break; }
                auto res = inv.splitStack(static_cast<std::size_t>(slotNum - 1), splitAmt);
                if (!res) std::cout << "Split failed: " << res.error() << "\n";
                else      std::cout << "Stack split successfully.\n";
                break;
            }
            case 28: {  // use recipe scroll
                std::cout << "Enter scroll id (e.g. recipe_scroll_steel_sword): ";
                std::string scrollId;
                std::getline(std::cin, scrollId);
                auto res = inv.learnFromScroll(scrollId);
                if (!res) std::cout << "Learn failed: " << res.error() << "\n";
                else      std::cout << "Learned recipe: " << res.value() << "!\n";
                break;
            }
            case 29: {  // known recipes
                std::cout << "\n--- Known Recipes ---\n";
                inv.printKnownRecipes(crafting);
                break;
            }
            case 26: {  // disenchant
                std::cout << "Enter item id to disenchant: ";
                std::string deid;
                std::getline(std::cin, deid);
                auto res = inv.disenchantItem(deid, factory, rng);
                if (!res) std::cout << "Disenchant failed: " << res.error() << "\n";
                else      std::cout << "Removed enchantment '" << res.value()
                                    << "'. Recovery stone added to inventory.\n";
                break;
            }
            case 27: {  // upgrade item
                std::cout << "Enter item id to upgrade: ";
                std::string upid;
                std::getline(std::cin, upid);
                auto res = inv.upgradeItem(upid, "upgrade_stone", 3);
                if (!res) std::cout << "Upgrade failed: " << res.error() << "\n";
                else      std::cout << "Item upgraded! (consumed 3x upgrade_stone)\n";
                break;
            }
            case 25: {  // compare item to equipped
                std::cout << "Enter item id to compare: ";
                std::string cmpId;
                std::getline(std::cin, cmpId);
                inv.compareToEquipped(cmpId);
                break;
            }
            case 21: {  // use consumable
                std::cout << "Enter consumable id (e.g. health_potion): ";
                std::string cid;
                std::getline(std::cin, cid);
                auto res = inv.useConsumable(cid);
                if (!res) {
                    std::cout << "Use failed: " << res.error() << "\n";
                } else {
                    auto stats = player.computeStats(inv, setMgr);
                    int gained = player.applyHeal(res.value(), stats);
                    std::cout << "Used " << cid << "! Restored " << gained << " HP. "
                              << "HP: " << player.currentHP() << "/" << stats.maxHP << "\n";
                }
                break;
            }
            default:
                std::cout << "Unknown option.\n";
        }
    }

    std::cout << "Good‑bye!\n";
    return 0;
}
