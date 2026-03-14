#pragma once

#include "item.hpp"
#include "item_factory.hpp"
#include "inventory.hpp"
#include "result.hpp"
#include "logger.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>

/*======================================================================
 *  Shop — buy / sell items using a gold currency
 *
 *  Base sell value is derived from item weight, rarity tier, and type.
 *  Buy price = sell value * buyMarkup (default 2.5×).
 *  Enchantments add a flat bonus per point to both prices.
 *====================================================================*/
class Shop {
public:
    explicit Shop(float buyMarkup = 2.5f) : buyMarkup_(buyMarkup) {}

    // -----------------------------------------------------------------
    //  Stock management
    // -----------------------------------------------------------------
    void stock(const Item& item) {
        stock_.push_back(item);
    }

    void stockFromFactory(ItemFactory& factory, int playerLevel, int count = 6) {
        stock_.clear();
        for (int i = 0; i < count; ++i) {
            auto res = factory.createRandomItem(playerLevel);
            if (res) stock_.push_back(res.value());
        }
        Log::info("Shop restocked with " + std::to_string(stock_.size()) + " items.");
    }

    const std::vector<Item>& getStock() const { return stock_; }

    // -----------------------------------------------------------------
    //  Pricing
    // -----------------------------------------------------------------
    int sellPrice(const Item& item) const {
        int base = baseValue(item);
        int enchBonus = enchantmentBonus(item);
        return std::max(1, base + enchBonus);
    }

    int buyPrice(const Item& item) const {
        return std::max(1, static_cast<int>(std::round(sellPrice(item) * buyMarkup_)));
    }

    // -----------------------------------------------------------------
    //  Transactions
    // -----------------------------------------------------------------
    // Player buys item from shop stock by index; gold is deducted from inventory.
    Result<void> buy(std::size_t stockIndex, Inventory& inv, int& playerGold) {
        if (stockIndex >= stock_.size())
            return Result<void>::err("invalid stock index");

        const Item& item = stock_[stockIndex];
        int price = buyPrice(item);

        if (playerGold < price)
            return Result<void>::err("not enough gold (need " + std::to_string(price)
                                     + ", have " + std::to_string(playerGold) + ")");

        auto addRes = inv.addItem(item);
        if (!addRes) return Result<void>::err("cannot carry item: " + addRes.error());

        playerGold -= price;
        Log::info("Bought '" + item.id + "' for " + std::to_string(price) + "g");
        stock_.erase(stock_.begin() + static_cast<std::ptrdiff_t>(stockIndex));
        return Result<void>::ok();
    }

    // Player sells one unit of itemId from their inventory; gold is added.
    Result<void> sell(const std::string& itemId, Inventory& inv, int& playerGold) {
        const auto& items = inv.getItems();
        auto it = std::find_if(items.begin(), items.end(),
            [&](const Item& i){ return i.id == itemId; });
        if (it == items.end())
            return Result<void>::err("item '" + itemId + "' not in inventory");

        int price = sellPrice(*it);
        auto rem = inv.removeItem(itemId, 1);
        if (!rem) return Result<void>::err("remove failed: " + rem.error());

        playerGold += price;
        Log::info("Sold '" + itemId + "' for " + std::to_string(price) + "g");
        return Result<void>::ok();
    }

    // Print current stock with buy prices
    void printStock() const {
        if (stock_.empty()) {
            std::cout << "  (shop is empty — ask the merchant to restock)\n";
            return;
        }
        for (std::size_t i = 0; i < stock_.size(); ++i) {
            std::cout << "  " << i + 1 << ") "
                      << stock_[i].getDescription()
                      << "  \x1B[33m[" << buyPrice(stock_[i]) << "g]\x1B[0m\n";
        }
    }

private:
    float buyMarkup_;
    std::vector<Item> stock_;

    int baseValue(const Item& item) const {
        int rarityMul = 1 + static_cast<int>(item.rarity); // 1..5
        int weight    = std::max(1, item.weightPerUnit());
        int typeBonus = 0;
        switch (item.type) {
            case ItemType::Weapon:     typeBonus = 10; break;
            case ItemType::Armor:      typeBonus =  8; break;
            case ItemType::Consumable: typeBonus =  3; break;
            case ItemType::Material:   typeBonus =  1; break;
            default:                   typeBonus =  5; break;
        }
        return (weight + typeBonus) * rarityMul;
    }

    int enchantmentBonus(const Item& item) const {
        int bonus = 0;
        for (const auto& e : item.enchantments)
            bonus += e.value * 2;
        return bonus;
    }
};
