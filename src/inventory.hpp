#pragma once

#include "item.hpp"
#include "item_factory.hpp"
#include "crafting.hpp"
#include "result.hpp"
#include "logger.hpp"

#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <string>
#include <cstddef>
#include <utility>

/*======================================================================
 *  6) Inventory – stacking, weight/slot limits, equip slots, persistence
 *====================================================================*/
class Inventory {
public:
    explicit Inventory(std::size_t slotLimit = 30, int weightLimit = 300)
        : slotLimit_(slotLimit), weightLimit_(weightLimit) {}

    Result<void> addItem(const Item& item) {
        Item toAdd = item; // work on a copy because we may split stacks

        // ---------- 1) capacity checks (weight + slots) ----------
        int neededWeight = toAdd.getWeight();
        if (totalWeight_ + neededWeight > weightLimit_)
            return Result<void>::err("weight limit exceeded");

        std::size_t neededSlots = 0;
        if (toAdd.maxStack > 1) {
            int remaining = toAdd.stackSize;
            for (const auto& existing : items_) {
                if (existing.id == toAdd.id && existing.stackSize < existing.maxStack) {
                    int freeSpace = existing.maxStack - existing.stackSize;
                    int use = std::min(freeSpace, remaining);
                    remaining -= use;
                    if (remaining == 0) break;
                }
            }
            if (remaining > 0)
                neededSlots = static_cast<std::size_t>((remaining + toAdd.maxStack - 1) / toAdd.maxStack);
        } else {
            neededSlots = static_cast<std::size_t>(toAdd.stackSize);
        }

        if (items_.size() + neededSlots > slotLimit_)
            return Result<void>::err("slot limit reached");

        // ---------- 2) actual insertion (weight updated) ----------
        if (toAdd.maxStack > 1) {
            for (auto& existing : items_) {
                if (existing.id == toAdd.id && existing.stackSize < existing.maxStack) {
                    int freeSpace = existing.maxStack - existing.stackSize;
                    int transfer = std::min(freeSpace, toAdd.stackSize);
                    existing.stackSize += transfer;
                    totalWeight_ += existing.weightPerUnit() * transfer;
                    toAdd.stackSize -= transfer;
                    if (toAdd.stackSize == 0) break;
                }
            }
        }

        while (toAdd.stackSize > 0) {
            int thisStackSize = std::min(toAdd.stackSize, toAdd.maxStack);
            Item singleStack = toAdd;
            singleStack.stackSize = thisStackSize;

            items_.push_back(singleStack);
            totalWeight_ += singleStack.getWeight();

            toAdd.stackSize -= thisStackSize;
        }

        return Result<void>::ok();
    }

    Result<void> removeItem(const std::string& id, int quantity = 1) {
        if (quantity <= 0) return Result<void>::ok();
        int remaining = quantity;

        for (auto it = items_.begin(); it != items_.end() && remaining > 0; ) {
            if (it->id == id) {
                if (it->stackSize > remaining) {
                    int unitWeight = it->weightPerUnit();
                    it->stackSize -= remaining;
                    totalWeight_ -= unitWeight * remaining;
                    remaining = 0;
                    break;
                } else {
                    totalWeight_ -= it->getWeight();
                    remaining -= it->stackSize;
                    it = items_.erase(it);
                    continue;
                }
            }
            ++it;
        }

        if (remaining > 0)
            return Result<void>::err("item not found in inventory");
        return Result<void>::ok();
    }

    int count(const std::string& id) const {
        int sum = 0;
        for (const auto& it : items_)
            if (it.id == id) sum += it.stackSize;
        return sum;
    }

    // -----------------------------------------------------------------
    //  Accessors for UI / other systems
    // -----------------------------------------------------------------
    int totalWeight() const { return totalWeight_; }
    size_t usedSlots() const { return items_.size(); }
    const std::vector<Item>& getItems() const { return items_; }
    const std::unordered_map<EquipSlot, std::unique_ptr<Item>>& getEquipment() const { return equipped_; }

    // -----------------------------------------------------------------
    //  Equipment handling
    // -----------------------------------------------------------------
    Result<void> equip(const std::string& id, int playerLevel = 1) {
        auto it = std::find_if(items_.begin(), items_.end(),
                               [&](const Item& i){ return i.id == id; });
        if (it == items_.end())
            return Result<void>::err("item not in inventory");

        if (it->levelReq > playerLevel)
            return Result<void>::err("your level is too low to equip this item");

        EquipSlot slot = slotForItem(*it);
        if (slot == EquipSlot::None)
            return Result<void>::err("item not equipable");

        // Unequip current item (if any) back into inventory
        if (auto* cur = equipped_[slot].get()) {
            totalWeight_ -= cur->getWeight();
            auto back = addItem(*cur);
            if (!back) {
                totalWeight_ += cur->getWeight();
                return Result<void>::err("cannot unequip existing item: " + back.error());
            }
        }

        // Move one instance (or the whole stack if non‑stackable)
        if (it->maxStack > 1 && it->stackSize > 1) {
            Item one = *it;
            one.stackSize = 1;
            int unitWeight = it->weightPerUnit();

            it->stackSize -= 1;
            totalWeight_ -= unitWeight;

            equipped_[slot] = std::make_unique<Item>(std::move(one));
            totalWeight_ += unitWeight;
        } else {
            int itemWeight = it->getWeight();
            totalWeight_ -= itemWeight;
            equipped_[slot] = std::make_unique<Item>(std::move(*it));
            items_.erase(it);
            totalWeight_ += itemWeight;
        }

        Log::info("Equipped '" + id + "' to slot " + toString(slot));
        return Result<void>::ok();
    }

    Result<void> unequip(EquipSlot slot) {
        auto it = equipped_.find(slot);
        if (it == equipped_.end() || !it->second)
            return Result<void>::err("slot empty");

        int eqWeight = it->second->getWeight();
        totalWeight_ -= eqWeight;

        auto back = addItem(*it->second);
        if (!back) {
            totalWeight_ += eqWeight;
            return Result<void>::err("cannot unequip: " + back.error());
        }

        it->second.reset();
        Log::info("Unequipped slot " + toString(slot));
        return Result<void>::ok();
    }

    const Item* getEquipped(EquipSlot slot) const {
        auto it = equipped_.find(slot);
        return (it != equipped_.end() && it->second) ? it->second.get() : nullptr;
    }

    // -----------------------------------------------------------------
    //  Crafting – uses ItemFactory + CraftingSystem
    // -----------------------------------------------------------------
    Result<void> craft(const std::string& resultId,
                       ItemFactory& factory,
                       const CraftingSystem& crafting,
                       int playerLevel = 1) {
        const Recipe* rec = crafting.get(resultId);
        if (!rec) return Result<void>::err("no recipe for '" + resultId + "'");

        // check ingredient availability
        for (auto& [ingId, qty] : rec->ingredients) {
            if (count(ingId) < qty)
                return Result<void>::err("missing ingredient '" + ingId + "' (need " + std::to_string(qty) + ")");
        }

        // create product
        auto prodRes = factory.create(resultId, playerLevel);
        if (!prodRes) return Result<void>::err("factory failed: " + prodRes.error());

        Item product = prodRes.value();
        product.stackSize = rec->resultCount;

        // ensure we have room for the product
        auto can = canAdd(product);
        if (!can) return Result<void>::err("no space/weight for crafted item");

        // consume ingredients
        for (auto& [ingId, qty] : rec->ingredients) {
            auto rem = removeItem(ingId, qty);
            if (!rem) return Result<void>::err("failed to consume '" + ingId + "': " + rem.error());
        }

        // store product
        auto addRes = addItem(product);
        if (!addRes) return Result<void>::err("failed to store crafted item: " + addRes.error());

        Log::info("Crafted '" + resultId + "' x" + std::to_string(product.stackSize));
        return Result<void>::ok();
    }

    // -----------------------------------------------------------------
    //  Persistence (save / load)
    // -----------------------------------------------------------------
    std::string serialize() const {
        json j;
        j["items"] = items_;
        json eq;
        for (const auto& [slot, ptr] : equipped_) {
            if (ptr) eq[toString(slot)] = *ptr;
            else    eq[toString(slot)] = nullptr;
        }
        j["equipment"] = eq;
        return j.dump(4);
    }

    Result<void> deserialize(const std::string& data) {
        json j;
        try { j = json::parse(data); }
        catch (const std::exception& e) { return Result<void>::err("JSON parse error: " + std::string(e.what())); }

        items_.clear();
        equipped_.clear();
        totalWeight_ = 0;

        if (!j.contains("items") || !j["items"].is_array())
            return Result<void>::err("missing or invalid 'items' array");

        for (const auto& elem : j["items"]) {
            try {
                Item it = elem.get<Item>();
                items_.push_back(it);
                totalWeight_ += it.getWeight();
            } catch (const std::exception& e) {
                Log::warn("Failed to load item: " + std::string(e.what()));
            }
        }

        if (j.contains("equipment") && j["equipment"].is_object()) {
            const json& eq = j["equipment"];
            for (auto it = eq.object_begin(); it != eq.object_end(); ++it) {
                const std::string& slotStr = it.key();
                EquipSlot slot = EquipSlot::None;
                if (slotStr == "Head")       slot = EquipSlot::Head;
                else if (slotStr == "Chest") slot = EquipSlot::Chest;
                else if (slotStr == "Legs")  slot = EquipSlot::Legs;
                else if (slotStr == "Weapon")slot = EquipSlot::Weapon;
                else if (slotStr == "Shield")slot = EquipSlot::Shield;
                else if (slotStr == "Accessory") slot = EquipSlot::Accessory;

                if (slot == EquipSlot::None) continue;

                if (!it.value().is_null()) {
                    try {
                        Item eqItem = it.value().get<Item>();
                        equipped_[slot] = std::make_unique<Item>(eqItem);
                        totalWeight_ += eqItem.getWeight();
                    } catch (const std::exception& e) {
                        Log::warn("Failed to load equipped item for " + slotStr + ": " + std::string(e.what()));
                    }
                }
            }
        }

        if (items_.size() > slotLimit_)
            Log::warn("Loaded inventory exceeds slot limit (" + std::to_string(items_.size()) +
                      " > " + std::to_string(slotLimit_) + ").");
        if (totalWeight_ > weightLimit_)
            Log::warn("Loaded inventory exceeds weight limit (" + std::to_string(totalWeight_) +
                      " > " + std::to_string(weightLimit_) + ").");

        return Result<void>::ok();
    }

    // -----------------------------------------------------------------
    //  Helper used by crafting – does the item *fit* into the inventory?
    // -----------------------------------------------------------------
    Result<void> canAdd(const Item& item) const {
        if (totalWeight_ + item.getWeight() > weightLimit_)
            return Result<void>::err("weight limit would be exceeded");

        std::size_t neededSlots = 0;
        if (item.maxStack > 1) {
            int remaining = item.stackSize;
            for (const auto& existing : items_) {
                if (existing.id == item.id && existing.stackSize < existing.maxStack) {
                    int freeSpace = existing.maxStack - existing.stackSize;
                    int use = std::min(freeSpace, remaining);
                    remaining -= use;
                    if (remaining == 0) break;
                }
            }
            if (remaining > 0)
                neededSlots = static_cast<std::size_t>((remaining + item.maxStack - 1) / item.maxStack);
        } else {
            neededSlots = static_cast<std::size_t>(item.stackSize);
        }

        if (items_.size() + neededSlots > slotLimit_)
            return Result<void>::err("no free inventory slot for the item");

        return Result<void>::ok();
    }

private:
    std::size_t slotLimit_;
    int weightLimit_;
    int totalWeight_{0};

    std::vector<Item> items_;
    std::unordered_map<EquipSlot, std::unique_ptr<Item>> equipped_;

    static EquipSlot slotForItem(const Item& it) {
        if (it.type == ItemType::Weapon)      return EquipSlot::Weapon;
        if (it.type == ItemType::Armor) {
            const std::string& id = it.id;
            if (id.find("helmet") != std::string::npos ||
                id.find("head")   != std::string::npos) return EquipSlot::Head;
            if (id.find("chest")  != std::string::npos ||
                id.find("armor")  != std::string::npos) return EquipSlot::Chest;
            if (id.find("leg")    != std::string::npos ||
                id.find("boots")  != std::string::npos) return EquipSlot::Legs;
            return EquipSlot::Chest;
        }
        if (it.id.find("shield") != std::string::npos) return EquipSlot::Shield;
        if (it.id.find("ring")   != std::string::npos ||
            it.id.find("amulet") != std::string::npos) return EquipSlot::Accessory;
        return EquipSlot::None;
    }
};
