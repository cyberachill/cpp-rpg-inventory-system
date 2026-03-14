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
#include <unordered_set>

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

        if (it->isBroken())
            return Result<void>::err("cannot equip a broken item — repair it first");

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
    //  Enchantment
    // -----------------------------------------------------------------
    // Apply a random enchantment from an enchanting stone to a target item.
    // Consumes one enchanting stone from the bag on success.
    Result<void> enchantItem(const std::string& targetId,
                             const std::string& stoneId,
                             std::mt19937& rng) {
        // locate stone
        auto stoneIt = std::find_if(items_.begin(), items_.end(),
            [&](const Item& i){ return i.id == stoneId; });
        if (stoneIt == items_.end())
            return Result<void>::err("enchanting stone '" + stoneId + "' not in inventory");
        // enchantment stones are Misc items with a special id prefix
        if (stoneIt->type != ItemType::Misc)
            return Result<void>::err("'" + stoneId + "' is not an enchanting stone");

        // locate target (bag or equipped)
        Item* target = nullptr;
        for (auto& item : items_)
            if (item.id == targetId) { target = &item; break; }
        if (!target) {
            for (auto& [slot, ptr] : equipped_)
                if (ptr && ptr->id == targetId) { target = ptr.get(); break; }
        }
        if (!target)
            return Result<void>::err("item '" + targetId + "' not found");
        if (target->type == ItemType::Material || target->type == ItemType::Consumable)
            return Result<void>::err("cannot enchant materials or consumables");

        // determine power tier from stone id
        bool advanced = (stoneId.find("advanced") != std::string::npos ||
                         stoneId.find("crystal")  != std::string::npos);
        int valMin = advanced ? 4 : 1;
        int valMax = advanced ? 9 : 4;

        static const std::array<Stat, 4> stats{
            Stat::Attack, Stat::Defense, Stat::Health, Stat::Mana};
        static const std::array<std::string, 5> elements{
            "", "Fire", "Ice", "Lightning", "Poison"};
        static const std::array<std::string, 8> suffixes{
            "of the Bear", "of the Fox", "of the Eagle",
            "of Power",    "of the Sage","of Warding",
            "of the Storm","of Fortune"};

        std::uniform_int_distribution<int> dv(valMin, valMax);
        std::uniform_int_distribution<int> ds(0, 3);
        std::uniform_int_distribution<int> de(0, 4);
        std::uniform_int_distribution<int> dn(0, 7);

        Enchantment e;
        e.stat    = stats[static_cast<std::size_t>(ds(rng))];
        e.value   = dv(rng);
        e.element = elements[static_cast<std::size_t>(de(rng))];
        e.name    = suffixes[static_cast<std::size_t>(dn(rng))];
        target->enchantments.push_back(e);

        // consume stone
        auto rem = removeItem(stoneId, 1);
        if (!rem) return Result<void>::err("failed to consume stone: " + rem.error());

        Log::info("Enchanted '" + targetId + "' with +" + std::to_string(e.value)
                  + " " + toString(e.stat));
        return Result<void>::ok();
    }

    // -----------------------------------------------------------------
    //  Recipe Discovery
    // -----------------------------------------------------------------

    // Pre-seed known recipes (called at game start for basic items)
    void learnRecipeById(const std::string& recipeId) {
        knownRecipes_.insert(recipeId);
    }

    // Consume a recipe scroll from inventory; returns the recipe id unlocked.
    // Scroll naming convention: "recipe_scroll_<resultId>"
    Result<std::string> learnFromScroll(const std::string& scrollId) {
        auto it = std::find_if(items_.begin(), items_.end(),
            [&](const Item& i){ return i.id == scrollId; });
        if (it == items_.end())
            return Result<std::string>::err("scroll '" + scrollId + "' not in inventory");
        if (it->type != ItemType::Misc)
            return Result<std::string>::err("'" + scrollId + "' is not a recipe scroll");

        // extract recipe id from scroll id: "recipe_scroll_X" → "X"
        const std::string prefix = "recipe_scroll_";
        if (scrollId.substr(0, prefix.size()) != prefix)
            return Result<std::string>::err("'" + scrollId + "' is not a recipe scroll");
        std::string recipeId = scrollId.substr(prefix.size());

        if (knownRecipes_.count(recipeId))
            return Result<std::string>::err("recipe '" + recipeId + "' already known");

        knownRecipes_.insert(recipeId);
        auto rem = removeItem(scrollId, 1);
        if (!rem) return Result<std::string>::err("failed to consume scroll: " + rem.error());

        Log::info("Learned recipe: " + recipeId);
        return Result<std::string>::ok(recipeId);
    }

    bool knowsRecipe(const std::string& id) const {
        // empty knownRecipes = discovery disabled (backwards-compat)
        return knownRecipes_.empty() || knownRecipes_.count(id) > 0;
    }

    void printKnownRecipes(const CraftingSystem& crafting) const {
        if (knownRecipes_.empty()) {
            std::cout << "  (All recipes available — no scroll system active)\n";
            return;
        }
        std::cout << "  Known recipes:\n";
        for (const auto& rid : knownRecipes_) {
            const Recipe* r = crafting.get(rid);
            if (r)
                std::cout << "    " << rid << " [" << r->category << "]\n";
            else
                std::cout << "    " << rid << " (recipe data missing)\n";
        }
    }

    // -----------------------------------------------------------------
    //  Disenchant + Item Upgrade
    // -----------------------------------------------------------------

    // Remove one random enchantment from the item and return a recovery material.
    // Gives back 1x enchanting_stone (basic) or enchanting_crystal (if rare+ enchant).
    // Returns the name of the enchantment removed, or an error string.
    Result<std::string> disenchantItem(const std::string& id,
                                       ItemFactory& factory,
                                       std::mt19937& rng) {
        // find in bag or equipped
        Item* target = nullptr;
        for (auto& item : items_)
            if (item.id == id) { target = &item; break; }
        if (!target) {
            for (auto& [slot, ptr] : equipped_)
                if (ptr && ptr->id == id) { target = ptr.get(); break; }
        }
        if (!target)
            return Result<std::string>::err("item '" + id + "' not found");
        if (target->enchantments.empty())
            return Result<std::string>::err("'" + target->name + "' has no enchantments");

        // pick a random enchantment
        std::uniform_int_distribution<std::size_t> di(
            0, target->enchantments.size() - 1);
        std::size_t idx = di(rng);
        Enchantment removed = target->enchantments[idx];
        target->enchantments.erase(target->enchantments.begin() +
                                   static_cast<std::ptrdiff_t>(idx));

        // determine recovery material
        bool advanced = (removed.value >= 5);
        std::string recovId = advanced ? "enchanting_crystal" : "enchanting_stone";
        auto recov = factory.create(recovId, 1);
        if (recov) {
            auto addRes = addItem(recov.value());
            if (!addRes)
                Log::warn("Disenchant recovery item dropped (no space): " + addRes.error());
        }

        std::string enchName = removed.name.empty()
                                ? toString(removed.stat)
                                : removed.name;
        Log::info("Disenchanted '" + id + "': removed '" + enchName +
                  "', returned " + recovId);
        return Result<std::string>::ok(enchName);
    }

    // Upgrade an item's primary combat stat by a fixed amount,
    // consuming `costQty` units of `costId` from inventory.
    // Weapon: +2 damage; Armor: +1 defense; per upgrade (up to 5 upgrades).
    Result<void> upgradeItem(const std::string& id,
                             const std::string& costId,
                             int costQty,
                             int maxUpgrades = 5) {
        Item* target = nullptr;
        for (auto& item : items_)
            if (item.id == id) { target = &item; break; }
        if (!target) {
            for (auto& [slot, ptr] : equipped_)
                if (ptr && ptr->id == id) { target = ptr.get(); break; }
        }
        if (!target)
            return Result<void>::err("item '" + id + "' not found");
        if (target->type != ItemType::Weapon && target->type != ItemType::Armor)
            return Result<void>::err("only weapons and armor can be upgraded");

        // track upgrades via a special enchantment named "Upgrade"
        int upgrades = 0;
        for (const auto& e : target->enchantments)
            if (e.name == "Upgrade") upgrades += e.value;
        if (upgrades >= maxUpgrades)
            return Result<void>::err("'" + target->name + "' is already at max upgrade tier ("
                                     + std::to_string(maxUpgrades) + ")");

        // material check
        if (count(costId) < costQty)
            return Result<void>::err("need " + std::to_string(costQty) + "x " + costId);

        // apply upgrade
        bool applied = std::visit([](auto& data) -> bool {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, WeaponData>) { data.damage  += 2; return true; }
            if constexpr (std::is_same_v<T, ArmorData>)  { data.defense += 1; return true; }
            return false;
        }, target->data);
        if (!applied)
            return Result<void>::err("item type not upgradeable");

        // record upgrade counter as hidden enchantment
        bool found = false;
        for (auto& e : target->enchantments) {
            if (e.name == "Upgrade") { e.value++; found = true; break; }
        }
        if (!found) {
            Enchantment e;
            e.stat  = Stat::Attack;
            e.value = 1;
            e.name  = "Upgrade";
            target->enchantments.push_back(e);
        }

        // consume materials
        auto rem = removeItem(costId, costQty);
        if (!rem) return Result<void>::err("failed to consume materials: " + rem.error());

        Log::info("Upgraded '" + id + "' (tier " + std::to_string(upgrades + 1) + ")");
        return Result<void>::ok();
    }

    // -----------------------------------------------------------------
    //  Item comparison
    // -----------------------------------------------------------------
    // Prints a side-by-side stat delta between the bag item `id` and
    // whatever is currently equipped in its natural slot.
    void compareToEquipped(const std::string& id) const {
        const Item* candidate = nullptr;
        for (const auto& item : items_)
            if (item.id == id) { candidate = &item; break; }
        if (!candidate) {
            std::cout << "  Item '" << id << "' not found in inventory.\n";
            return;
        }

        EquipSlot slot = const_cast<Inventory*>(this)->slotForItem(*candidate);
        if (slot == EquipSlot::None) {
            std::cout << "  '" << candidate->name << "' cannot be equipped.\n";
            return;
        }

        const Item* current = getEquipped(slot);

        std::cout << "\n  \x1B[1m[Compare] " << candidate->getDescription() << "\x1B[0m\n";
        if (!current) {
            std::cout << "  " << toString(slot) << " slot is empty (nothing to compare).\n";
        } else {
            std::cout << "  vs  " << current->getDescription() << "\n";
        }

        // Stat delta helper
        auto printDelta = [](const std::string& label, int newVal, int oldVal) {
            int delta = newVal - oldVal;
            if (delta == 0) return;
            std::string col = (delta > 0) ? "\x1B[32m" : "\x1B[31m";
            std::string sign = (delta > 0) ? "+" : "";
            std::cout << "    " << label << ": " << col
                      << sign << delta << " (" << oldVal << " → " << newVal
                      << ")\x1B[0m\n";
        };

        int newDmg = 0, curDmg = 0, newDef = 0, curDef = 0;
        int newDur = 0, curDur = 0, newWgt = 0, curWgt = 0;

        if (const auto* wd = std::get_if<WeaponData>(&candidate->data)) {
            newDmg = wd->damage; newDur = wd->durability; newWgt = wd->weight; }
        if (const auto* ad = std::get_if<ArmorData>(&candidate->data)) {
            newDef = ad->defense; newDur = ad->durability; newWgt = ad->weight; }
        if (current) {
            if (const auto* wd = std::get_if<WeaponData>(&current->data)) {
                curDmg = wd->damage; curDur = wd->durability; curWgt = wd->weight; }
            if (const auto* ad = std::get_if<ArmorData>(&current->data)) {
                curDef = ad->defense; curDur = ad->durability; curWgt = ad->weight; }
        }

        if (newDmg || curDmg)  printDelta("Damage",    newDmg, curDmg);
        if (newDef || curDef)  printDelta("Defense",   newDef, curDef);
        if (newDur || curDur)  printDelta("Durability",newDur, curDur);
        printDelta("Weight", newWgt, curWgt);

        // Enchantment count delta
        int newEnc = static_cast<int>(candidate->enchantments.size());
        int curEnc = current ? static_cast<int>(current->enchantments.size()) : 0;
        if (newEnc != curEnc) {
            std::string col = (newEnc > curEnc) ? "\x1B[32m" : "\x1B[31m";
            std::cout << "    Enchantments: " << col
                      << curEnc << " → " << newEnc << "\x1B[0m\n";
        }
    }

    // -----------------------------------------------------------------
    //  Inventory UX — sort, filter, stack split
    // -----------------------------------------------------------------
    enum class SortKey { Name, Type, Rarity, Weight };

    void sort(SortKey key = SortKey::Type) {
        std::sort(items_.begin(), items_.end(), [key](const Item& a, const Item& b) {
            switch (key) {
                case SortKey::Name:
                    return a.name < b.name;
                case SortKey::Rarity:
                    // rarer first
                    return static_cast<int>(a.rarity) > static_cast<int>(b.rarity);
                case SortKey::Weight:
                    return a.getWeight() > b.getWeight();
                default: // Type
                    if (a.type != b.type)
                        return static_cast<int>(a.type) < static_cast<int>(b.type);
                    return static_cast<int>(a.rarity) > static_cast<int>(b.rarity);
            }
        });
    }

    // Returns pointers to items whose name or id contains the query (case-insensitive)
    std::vector<const Item*> filter(const std::string& query) const {
        std::string lq = query;
        std::transform(lq.begin(), lq.end(), lq.begin(), ::tolower);
        std::vector<const Item*> result;
        for (const auto& item : items_) {
            std::string ln = item.name;
            std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
            if (ln.find(lq) != std::string::npos ||
                item.id.find(lq) != std::string::npos)
                result.push_back(&item);
        }
        return result;
    }

    // Split `amount` units from the stack at index `slotIdx` into a new slot
    Result<void> splitStack(std::size_t slotIdx, int amount) {
        if (slotIdx >= items_.size())
            return Result<void>::err("invalid slot index");
        Item& src = items_[slotIdx];
        if (src.maxStack <= 1)
            return Result<void>::err("'" + src.name + "' is not stackable");
        if (amount <= 0 || amount >= src.stackSize)
            return Result<void>::err("amount must be 1.." + std::to_string(src.stackSize - 1));
        if (items_.size() >= slotLimit_)
            return Result<void>::err("no free slot for split stack");

        int unitWeight = src.weightPerUnit();
        src.stackSize -= amount;
        totalWeight_  -= unitWeight * amount;

        Item split  = src;
        split.stackSize = amount;
        items_.push_back(split);
        totalWeight_ += unitWeight * amount;

        Log::info("Split " + std::to_string(amount) + "x '" + src.id + "'");
        return Result<void>::ok();
    }

    // -----------------------------------------------------------------
    //  Consumables
    // -----------------------------------------------------------------
    // Returns the healAmount of the consumable (caller applies it to Player).
    // Consumes one unit from the stack.
    Result<int> useConsumable(const std::string& id) {
        auto it = std::find_if(items_.begin(), items_.end(),
            [&](const Item& i){ return i.id == id; });
        if (it == items_.end())
            return Result<int>::err("item '" + id + "' not in inventory");
        if (it->type != ItemType::Consumable)
            return Result<int>::err("'" + id + "' is not a consumable");
        const auto* cd = std::get_if<ConsumableData>(&it->data);
        if (!cd) return Result<int>::err("internal error: bad consumable data");

        int amount = cd->healAmount;
        auto rem = removeItem(id, 1);
        if (!rem) return Result<int>::err("failed to consume: " + rem.error());

        Log::info("Used consumable '" + id + "' (heal " + std::to_string(amount) + ")");
        return Result<int>::ok(amount);
    }

    // -----------------------------------------------------------------
    //  Durability management
    // -----------------------------------------------------------------
    Result<void> degradeEquipped(EquipSlot slot, int amount = 1) {
        auto it = equipped_.find(slot);
        if (it == equipped_.end() || !it->second)
            return Result<void>::err("no item equipped in that slot");
        it->second->degrade(amount);
        if (it->second->isBroken())
            Log::warn("'" + it->second->name + "' has broken!");
        return Result<void>::ok();
    }

    // Direct repair by amount (e.g. paid smithy). Works on bag and equipped slots.
    Result<void> repairItem(const std::string& id, int amount) {
        for (auto& item : items_) {
            if (item.id == id) {
                if (!item.repair(amount))
                    return Result<void>::err("item has no durability to restore");
                Log::info("Repaired '" + id + "' by " + std::to_string(amount));
                return Result<void>::ok();
            }
        }
        for (auto& [slot, ptr] : equipped_) {
            if (ptr && ptr->id == id) {
                if (!ptr->repair(amount))
                    return Result<void>::err("item has no durability to restore");
                Log::info("Repaired equipped '" + id + "' by " + std::to_string(amount));
                return Result<void>::ok();
            }
        }
        return Result<void>::err("item '" + id + "' not found");
    }

    // Consume one repair kit from bag and apply its repairAmount to the target item
    Result<void> useRepairKit(const std::string& kitId, const std::string& targetId) {
        auto kitIt = std::find_if(items_.begin(), items_.end(),
            [&](const Item& i){ return i.id == kitId; });
        if (kitIt == items_.end())
            return Result<void>::err("repair kit '" + kitId + "' not in inventory");
        const auto* misc = std::get_if<MiscData>(&kitIt->data);
        if (!misc || misc->repairAmount <= 0)
            return Result<void>::err("'" + kitId + "' is not a repair kit");

        int amount = misc->repairAmount;
        auto res = repairItem(targetId, amount);
        if (!res) return res;

        auto rem = removeItem(kitId, 1);
        if (!rem) return Result<void>::err("failed to consume kit: " + rem.error());

        Log::info("Used '" + kitId + "' to repair '" + targetId + "'");
        return Result<void>::ok();
    }

    // -----------------------------------------------------------------
    //  Crafting – uses ItemFactory + CraftingSystem + CraftingMastery
    // -----------------------------------------------------------------
    struct CraftResult {
        bool         success{false};
        CraftQuality quality{CraftQuality::Normal};
        bool         leveledUp{false};
        std::string  errorMsg;
    };

    CraftResult craft(const std::string& resultId,
                      ItemFactory&      factory,
                      const CraftingSystem& crafting,
                      CraftingMastery&  mastery,
                      std::mt19937&     rng,
                      int               playerLevel = 1)
    {
        CraftResult out;
        const Recipe* rec = crafting.get(resultId);
        if (!rec) { out.errorMsg = "no recipe for '" + resultId + "'"; return out; }

        // discovery check
        if (!knowsRecipe(resultId)) {
            out.errorMsg = "recipe '" + resultId + "' not yet discovered — find a scroll!";
            return out;
        }

        // level check
        if (playerLevel < rec->levelReq) {
            out.errorMsg = "requires player level " + std::to_string(rec->levelReq);
            return out;
        }
        // mastery check
        if (mastery.level(rec->category) < rec->masteryReq) {
            out.errorMsg = "requires " + rec->category + " mastery level "
                           + std::to_string(rec->masteryReq);
            return out;
        }

        // ingredient check
        for (auto& [ingId, qty] : rec->ingredients) {
            if (count(ingId) < qty) {
                out.errorMsg = "missing '" + ingId + "' (need " + std::to_string(qty) + ")";
                return out;
            }
        }

        // create product template
        auto prodRes = factory.create(resultId, playerLevel);
        if (!prodRes) { out.errorMsg = "factory failed: " + prodRes.error(); return out; }

        Item product = prodRes.value();
        product.stackSize = rec->resultCount;

        // space check
        auto can = canAdd(product);
        if (!can) { out.errorMsg = "no space/weight for crafted item"; return out; }

        // consume ingredients (before success roll — like real crafting)
        for (auto& [ingId, qty] : rec->ingredients) {
            auto rem = removeItem(ingId, qty);
            if (!rem) { out.errorMsg = "failed to consume '" + ingId + "'"; return out; }
        }

        // success roll
        float chance = mastery.successChance(*rec);
        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        if (roll(rng) > chance) {
            Log::info("Craft failed for '" + resultId + "' (ingredients consumed)");
            out.errorMsg = "crafting failed — materials were lost";
            return out;
        }

        // quality roll
        CraftQuality quality = mastery.rollQuality(*rec, rng);
        out.quality = quality;

        // apply quality multiplier to item stats
        float mul = qualityMultiplier(quality);
        if (mul > 1.0f) {
            std::visit([mul](auto& data) {
                using T = std::decay_t<decltype(data)>;
                if constexpr (std::is_same_v<T, WeaponData>) {
                    data.damage = static_cast<int>(data.damage * mul);
                    if (data.maxDurability > 0)
                        data.maxDurability = data.durability = static_cast<int>(data.maxDurability * mul);
                } else if constexpr (std::is_same_v<T, ArmorData>) {
                    data.defense = static_cast<int>(data.defense * mul);
                    if (data.maxDurability > 0)
                        data.maxDurability = data.durability = static_cast<int>(data.maxDurability * mul);
                } else if constexpr (std::is_same_v<T, ConsumableData>) {
                    data.healAmount = static_cast<int>(data.healAmount * mul);
                }
            }, product.data);
        }

        // masterwork items get a free enchantment
        if (quality == CraftQuality::Masterwork &&
            (product.type == ItemType::Weapon || product.type == ItemType::Armor)) {
            static const std::array<Stat,4> stats{Stat::Attack,Stat::Defense,Stat::Health,Stat::Mana};
            std::uniform_int_distribution<int> ds(0,3), dv(3,6);
            Enchantment e;
            e.stat  = stats[static_cast<std::size_t>(ds(rng))];
            e.value = dv(rng);
            e.name  = "of Mastery";
            product.enchantments.push_back(e);
        }

        auto addRes = addItem(product);
        if (!addRes) { out.errorMsg = "failed to store crafted item: " + addRes.error(); return out; }

        out.success  = true;
        out.leveledUp = mastery.awardXP(rec->category, 10);
        Log::info("Crafted '" + resultId + "' [" + toString(quality) + "] x"
                  + std::to_string(product.stackSize));
        return out;
    }

    // -----------------------------------------------------------------
    //  Persistence (save / load)
    // -----------------------------------------------------------------
    std::string serialize() const {
        json j;
        json itemArr = json::array();
        for (const auto& item : items_) {
            json jItem;
            to_json(jItem, item);
            itemArr.push_back(std::move(jItem));
        }
        j["items"] = itemArr;
        json eq;
        for (const auto& [slot, ptr] : equipped_) {
            if (ptr) {
                json jEq;
                to_json(jEq, *ptr);
                eq[toString(slot)] = std::move(jEq);
            } else {
                eq[toString(slot)] = nullptr;
            }
        }
        j["equipment"] = eq;
        if (!knownRecipes_.empty()) {
            json krArr = json::array();
            for (const auto& r : knownRecipes_) krArr.push_back(r);
            j["knownRecipes"] = krArr;
        }
        return j.dump(4);
    }

    Result<void> deserialize(const std::string& data) {
        json j;
        try { j = json_parse(data); }
        catch (const std::exception& e) { return Result<void>::err("JSON parse error: " + std::string(e.what())); }

        items_.clear();
        equipped_.clear();
        knownRecipes_.clear();
        totalWeight_ = 0;

        if (!j.contains("items") || !j["items"].is_array())
            return Result<void>::err("missing or invalid 'items' array");

        for (const auto& elem : j["items"]) {
            try {
                Item it{};
                from_json(elem, it);
                items_.push_back(it);
                totalWeight_ += it.getWeight();
            } catch (const std::exception& e) {
                Log::warn("Failed to load item: " + std::string(e.what()));
            }
        }

        if (j.contains("equipment") && j["equipment"].is_object()) {
            const json& eq = j["equipment"];
            for (auto& [slotStr, slotVal] : eq.items()) {
                EquipSlot slot = stringToEquipSlot(slotStr);
                if (slot == EquipSlot::None) continue;

                if (!slotVal.is_null()) {
                    try {
                        Item eqItem{};
                        from_json(slotVal, eqItem);
                        equipped_[slot] = std::make_unique<Item>(eqItem);
                        totalWeight_ += eqItem.getWeight();
                    } catch (const std::exception& e) {
                        Log::warn("Failed to load equipped item for " + slotStr + ": " + std::string(e.what()));
                    }
                }
            }
        }

        if (j.contains("knownRecipes") && j["knownRecipes"].is_array()) {
            for (const auto& r : j["knownRecipes"])
                knownRecipes_.insert(r.get<std::string>());
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
    std::unordered_set<std::string> knownRecipes_;

    // Returns the preferred slot; for Ring1/Ring2 the caller may promote to Ring2.
    EquipSlot slotForItem(const Item& it) const {
        if (it.type == ItemType::Weapon)      return EquipSlot::Weapon;
        if (it.type == ItemType::Armor) {
            const std::string& id = it.id;
            if (id.find("helmet") != std::string::npos ||
                id.find("head")   != std::string::npos)  return EquipSlot::Head;
            if (id.find("chest")  != std::string::npos ||
                id.find("plate")  != std::string::npos)  return EquipSlot::Chest;
            if (id.find("leg")    != std::string::npos)  return EquipSlot::Legs;
            if (id.find("boots")  != std::string::npos ||
                id.find("feet")   != std::string::npos)  return EquipSlot::Feet;
            if (id.find("glove")  != std::string::npos ||
                id.find("hands")  != std::string::npos ||
                id.find("gauntlet") != std::string::npos) return EquipSlot::Hands;
            return EquipSlot::Chest;
        }
        if (it.id.find("shield") != std::string::npos)  return EquipSlot::Shield;
        if (it.id.find("ring")   != std::string::npos) {
            // prefer Ring1; promote to Ring2 if Ring1 is occupied
            auto r1 = equipped_.find(EquipSlot::Ring1);
            if (r1 == equipped_.end() || !r1->second)   return EquipSlot::Ring1;
            return EquipSlot::Ring2;
        }
        if (it.id.find("amulet") != std::string::npos)  return EquipSlot::Accessory;
        return EquipSlot::None;
    }
};
