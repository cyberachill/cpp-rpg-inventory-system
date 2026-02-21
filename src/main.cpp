#include "inventory.hpp"
#include "item_factory.hpp"
#include "crafting.hpp"
#include "logger.hpp"

#include <iostream>
#include <fstream>
#include <limits>
#include <string>

int main() {
    Log::setFile("game.log");               // isteğe bağlı dosya logu
    ItemFactory   factory;
    CraftingSystem crafting;

    if (auto r = factory.loadTemplates("templates.json"); !r) {
        Log::error("Cannot continue without item templates: " + r.error());
        return 1;
    }
    if (auto r = crafting.loadFromFile("recipes.json"); !r) {
        Log::error("Cannot continue without recipes: " + r.error());
        return 1;
    }

    Inventory inv(30, 300);                  // 30 slot, 300 ağırlık limiti
    int playerLevel = 5;

    while (true) {
        std::cout << "\n--- MENU ---------------------------------------------------\n";
        std::cout << "1) Show inventory\n";
        std::cout << "2) Show equipment\n";
        std::cout << "3) Add random loot\n";
        std::cout << "4) Craft item\n";
        std::cout << "5) Equip item\n";
        std::cout << "6) Unequip slot\n";
        std::cout << "7) Save game\n";
        std::cout << "8) Load game\n";
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
                auto res = factory.createRandomItem(playerLevel);
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
                auto craftRes = inv.craft(rid, factory, crafting, playerLevel);
                if (!craftRes) std::cout << "Craft failed: " << craftRes.error() << "\n";
                else           std::cout << "Craft succeeded!\n";
                break;
            }
            case 5: {   // ekipana tak
                std::cout << "Enter inventory item id to equip: ";
                std::string iid;
                std::getline(std::cin, iid);
                auto eqRes = inv.equip(iid, playerLevel);
                if (!eqRes) std::cout << "Equip failed: " << eqRes.error() << "\n";
                else        std::cout << "Equipped successfully.\n";
                break;
            }
            case 6: {   // ekipmanı çıkar
                std::cout << "Enter slot name (Head, Chest, Legs, Weapon, Shield, Accessory): ";
                std::string slotStr;
                std::getline(std::cin, slotStr);
                EquipSlot slot = EquipSlot::None;
                if (slotStr == "Head")       slot = EquipSlot::Head;
                else if (slotStr == "Chest") slot = EquipSlot::Chest;
                else if (slotStr == "Legs")  slot = EquipSlot::Legs;
                else if (slotStr == "Weapon")slot = EquipSlot::Weapon;
                else if (slotStr == "Shield")slot = EquipSlot::Shield;
                else if (slotStr == "Accessory") slot = EquipSlot::Accessory;
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
                if (!out) {
                    std::cout << "Cannot open save file.\n";
                    break;
                }
                out << inv.serialize();
                std::cout << "Game saved to savegame.json\n";
                break;
            }
            case 8: {   // yükle
                std::ifstream in("savegame.json");
                if (!in) {
                    std::cout << "Cannot open save file.\n";
                    break;
                }
                std::string content((std::istreambuf_iterator<char>(in)), {});
                auto loadRes = inv.deserialize(content);
                if (!loadRes) std::cout << "Load failed: " << loadRes.error() << "\n";
                else          std::cout << "Game loaded.\n";
                break;
            }
            default:
                std::cout << "Unknown option.\n";
        }
    }

    std::cout << "Good‑bye!\n";
    return 0;
}
