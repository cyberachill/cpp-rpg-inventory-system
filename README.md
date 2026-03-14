# RPG Inventory & Crafting System

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.10%2B-green.svg?logo=cmake)](https://cmake.org/)
[![nlohmann/json](https://img.shields.io/badge/nlohmann%2Fjson-3.10.5-yellow.svg)](https://github.com/nlohmann/json)
[![License: MIT](https://img.shields.io/badge/License-MIT-lightgrey.svg)](LICENSE)

A **complete, production-quality RPG inventory backend** written in modern C++17. Features turn-based combat, a full crafting pipeline with recipe discovery, armor set bonuses, enchantments, item upgrades, and a persistent save system — all driven by JSON data files.

---

## Table of Contents

1. [Demo](#demo)
2. [Features](#features)
3. [Architecture](#architecture)
4. [Directory Layout](#directory-layout)
5. [Core Modules](#core-modules)
6. [Data Format](#data-format)
7. [Building](#building)
8. [Running](#running)
9. [Extending](#extending)
10. [License](#license)

---

## Demo

### Combat System
![Combat Demo](docs/assets/demo_combat.gif)

### Crafting, Recipe Discovery & Set Bonuses
![Crafting Demo](docs/assets/demo_crafting.gif)

### Inventory Management — Sort, Filter, Compare, Upgrade
![Inventory Demo](docs/assets/demo_inventory.gif)

---

## Features

| # | Feature | Details |
|---|---------|---------|
| A | **Result\<T\> error handling** | Monadic `Result<T>` / `Result<void>` — no exceptions, full error propagation |
| B | **Custom JSON engine** | Recursive-descent parser in a single header; mirrors nlohmann/json ADL |
| C | **Thread-safe logger** | `Log::info/warn/error` with optional file sink and global mutex |
| D | **Rarity system** | 5 tiers (Common → Legendary); weighted RNG, stat scaling, ANSI color output |
| E | **Item factory** | Template-based item creation from `data/templates.json`; per-rarity stat multipliers |
| F | **Armor set bonuses** | Equip N pieces of the same set to unlock threshold stat buffs (`iron_set`, `shadow_set`) |
| G | **Consumable use** | `useConsumable(id)` heals player HP; respects current HP cap |
| H | **Extended equip slots** | Head, Chest, Legs, Feet, Hands, Weapon, Shield, Ring1, Ring2, Accessory |
| I | **Sort / Filter / Split** | `sort(SortKey)` by name/type/rarity/weight; `filter(query)` substring search; `splitStack` |
| J | **Compare to equipped** | Side-by-side stat delta display with green/red highlights |
| K | **Enchantments** | Random stat bonuses on items; `disenchantItem` recovers a stone; `upgradeItem` up to +5 tiers |
| L | **Recipe discovery** | `craft()` gated on `knowsRecipe()`; learn via scrolls dropped in loot tables |
| M | **Turn-based combat** | 6 enemy types, HP bars, crit hits, flee mechanic, weapon durability drain, loot drops |
| N | **Weapon type system** | OneHanded, TwoHanded, Ranged, Shield, Unarmed; TwoHanded ↔ Shield slot mutual exclusion |
| — | **Crafting mastery** | Per-recipe craft counter; mastery level printed in character sheet |
| — | **Loot tables** | Data-driven drop pools (`data/loot_tables.json`) for chests, bosses, enemies |
| — | **Persistent save/load** | JSON save file stores inventory, equipment, known recipes, gold, and player level |

---

## Architecture

```
main.cpp
  │
  ├── Player (player.hpp)          — stats, HP, level, character sheet
  │     └── SetBonusManager        — active set bonuses from equipped armor
  │
  ├── Inventory (inventory.hpp)    — add/remove, equip, craft, sort, filter, persist
  │     ├── ItemFactory            — creates items from templates + rarity RNG
  │     ├── CraftingSystem         — recipe loading, ingredient validation
  │     └── LootTableManager       — weighted random loot drops
  │
  ├── CombatEngine (combat.hpp)    — turn loop, enemy catalogue, flee/crit logic
  │
  └── Data layer (JSON files)
        ├── data/templates.json    — item base definitions
        ├── data/recipes.json      — crafting recipes
        ├── data/loot_tables.json  — drop pool definitions
        └── data/set_bonuses.json  — armor set threshold bonuses
```

---

## Directory Layout

```
cpp-rpg-inventory-system/
├── CMakeLists.txt
├── README.md
├── data/
│   ├── loot_tables.json       ← drop pool definitions
│   ├── recipes.json           ← crafting recipes
│   ├── set_bonuses.json       ← armor set bonus thresholds
│   └── templates.json         ← base item definitions
├── docs/
│   └── assets/
│       ├── demo_combat.gif
│       ├── demo_crafting.gif
│       └── demo_inventory.gif
└── src/
    ├── combat.hpp             ← turn-based combat engine
    ├── crafting.hpp           ← data-driven recipe system
    ├── enchantment.hpp        ← enchantment structs + display
    ├── enums.hpp              ← ItemType, Rarity, EquipSlot, WeaponType, Stat
    ├── inventory.hpp          ← core inventory: add/remove/equip/craft/persist
    ├── item.hpp               ← Item struct, payload variant, JSON conversion
    ├── item_factory.hpp       ← template loader + random item creation
    ├── json.hpp               ← single-header JSON parser/serializer
    ├── logger.hpp             ← thread-safe log sink
    ├── loot_table.hpp         ← weighted loot table loader
    ├── main.cpp               ← interactive CLI demo
    ├── player.hpp             ← Player class, PlayerStats, computeStats
    ├── result.hpp             ← Result<T> monadic error type
    ├── set_bonus.hpp          ← SetBonusManager, armor set detection
    └── shop.hpp               ← buy/sell shop with gold economy
```

---

## Core Modules

### `result.hpp` — `Result<T>`
Functional error handling without exceptions. Every public API returns a `Result`.

```cpp
Result<Item> r = factory.create("iron_sword", player.level());
if (!r) {
    Log::warn(r.error());
} else {
    inventory.addItem(r.value());
}
```

Specialized `Result<void>` for operations that succeed or fail without a return value.

---

### `item.hpp` — Item model
Type-erased payload via `std::variant<WeaponData, ArmorData, ConsumableData, MaterialData, MiscData>`. Key fields:

| Field | Type | Purpose |
|-------|------|---------|
| `id` | `string` | Unique template identifier |
| `rarity` | `Rarity` | Scales stats; drives color output |
| `stackSize` | `int` | Current stack count |
| `enchantments` | `vector<Enchantment>` | Applied stat bonuses |
| `setId` | `string` | Armor set membership tag |
| `data` | `ItemPayload` | Weapon/Armor/Consumable/Material/Misc payload |

`WeaponData` carries a `weaponType` field (`OneHanded`, `TwoHanded`, `Ranged`, `Shield`, `Unarmed`).

---

### `inventory.hpp` — Inventory engine
The central class. Highlights:

- **`addItem`** — weight + slot checks, auto-stack merging
- **`equip(id)`** — routes to correct `EquipSlot`, enforces TwoHanded ↔ Shield exclusion, auto-promotes Ring slots
- **`craft(resultId)`** — checks `knowsRecipe()`, validates ingredients, creates via factory
- **`sort(SortKey)`** — in-place sort by name / type / rarity / weight
- **`filter(query)`** — returns matching items without modifying inventory
- **`compareToEquipped(id)`** — prints colored stat delta vs. currently equipped item
- **`upgradeItem(id)`** — costs 3× `upgrade_stone`, boosts weapon DMG or armor DEF up to 5 tiers
- **`disenchantItem(id)`** — removes a random enchantment, returns an `enchanting_stone`
- **`serialize` / `deserialize`** — full state round-trip including known recipes

---

### `player.hpp` — Player & stats
```cpp
Player player(/*level=*/5);
PlayerStats stats = player.computeStats(inventory, setMgr);
// stats.attack, stats.defense, stats.maxHP, stats.speed, stats.critChance
player.printSheet(inventory, setMgr);   // formatted character sheet
```
`computeStats` accumulates: base stats → equipped item bonuses → enchantment bonuses → active set bonuses.

---

### `combat.hpp` — Turn-based combat

Enemy catalogue (6 types): Goblin, Skeleton, Orc, Dark Mage, Dragon Whelp, Shadow Assassin.

```
[Combat] You attack Goblin for 14 damage! (Goblin: 11/25 HP)
[Combat] Goblin attacks you for 6 damage! (You: 44/50 HP)
> a  — attack
> h  — use health potion
> f  — flee (40% success)
```

- Damage variance ±20%; critical hits (based on `critChance`) deal 1.5× damage
- Equipped weapon degrades 2 durability per attack
- Defeat restores player to 1 HP and skips loot
- Gold and item drops fed back into inventory on victory

---

### `set_bonus.hpp` — Armor set system

```jsonc
// data/set_bonuses.json
{
  "iron_set": {
    "bonuses": [
      { "piecesRequired": 2, "stats": [{"stat": "Defense", "value": 5}] },
      { "piecesRequired": 3, "stats": [
          {"stat": "Defense", "value": 10},
          {"stat": "Attack",  "value": 15},
          {"stat": "MaxHP",   "value": 20}
      ]}
    ]
  }
}
```

`SetBonusManager::computeActive(equipped)` counts set pieces from the equipped map and returns all triggered bonus lists.

---

## Data Format

### `data/templates.json` (excerpt)
```jsonc
[
  {
    "id": "iron_sword",
    "name": "Iron Sword",
    "type": "Weapon",
    "rarity": "Common",
    "levelReq": 1,
    "maxStack": 1,
    "data": { "damage": 10, "durability": 80, "maxDurability": 80,
              "weight": 15, "weaponType": "OneHanded" }
  },
  {
    "id": "great_sword",
    "name": "Great Sword",
    "type": "Weapon",
    "rarity": "Rare",
    "levelReq": 5,
    "maxStack": 1,
    "data": { "damage": 22, "durability": 120, "maxDurability": 120,
              "weight": 30, "weaponType": "TwoHanded" }
  }
]
```

### `data/recipes.json` (excerpt)
```jsonc
[
  {
    "resultId": "iron_ingot",
    "resultCount": 1,
    "ingredients": { "iron_ore": 2, "coal": 1 }
  },
  {
    "resultId": "health_potion",
    "resultCount": 1,
    "ingredients": { "herb": 2, "water_flask": 1 }
  }
]
```

### `data/loot_tables.json` (excerpt)
```jsonc
[
  {
    "tableId": "dungeon_chest",
    "rolls": 2,
    "entries": [
      { "itemId": "iron_sword",              "weight": 20 },
      { "itemId": "health_potion",           "weight": 30 },
      { "itemId": "recipe_scroll_steel_sword","weight": 10 }
    ]
  }
]
```

---

## Building

**Requirements:** C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+), CMake 3.10+, nlohmann/json 3.10.5.

```bash
# Clone
git clone https://github.com/sa-aris/cpp-rpg-inventory-system.git
cd cpp-rpg-inventory-system

# Install nlohmann/json (Ubuntu/Debian)
sudo apt-get install nlohmann-json3-dev

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

On macOS:
```bash
brew install nlohmann-json
```

On Windows (vcpkg):
```powershell
vcpkg install nlohmann-json
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake ..
```

---

## Running

The executable reads data files relative to itself — run from the `build/` directory:

```bash
./RPGInventory
```

All actions are logged to `game.log` in the working directory.

### Menu reference

| Option | Action |
|--------|--------|
| `1` | View inventory |
| `2` | View equipped gear |
| `3` | Roll random loot |
| `4` | Craft item |
| `5` | Equip item |
| `6` | Unequip slot |
| `7` | Save game |
| `8` | Load game |
| `9` | Strike (test damage) |
| `10` | Use consumable |
| `11` | Buy from shop |
| `12` | Sell item |
| `13` | Repair item |
| `14` | Enchant item |
| `15` | Open loot table |
| `16` | Crafting mastery |
| `17` | Level up |
| `18` | Show set bonuses |
| `20` | Character sheet |
| `22` | Sort inventory |
| `23` | Filter inventory |
| `24` | Split stack |
| `25` | Compare to equipped |
| `26` | Disenchant item |
| `27` | Upgrade item |
| `28` | Learn recipe scroll |
| `29` | Show known recipes |
| `30` | Start combat |
| `0` | Quit |

---

## Extending

| Goal | What to change |
|------|----------------|
| New item type | Add payload struct + `to_json`/`from_json` + branch in `getDescription` and `slotForItem` |
| New stat | Add value to `Stat` enum, extend `PlayerStats::add`, update enchantment/set bonus handling |
| New enemy | Add entry to `enemyCatalogue()` in `combat.hpp` |
| New equip slot | Add to `EquipSlot` enum, update `toString`, adjust `slotForItem` |
| Complex recipe | Add `requiredLevel` or `requiredMastery` fields to `Recipe`, validate in `craft()` |
| GUI integration | Replace `main.cpp` with your render loop; all core headers are UI-agnostic |
| Unit tests | Each header is independently includable — wire into GoogleTest or Catch2 directly |

---

## License

Released under the [MIT License](LICENSE). Free to use, modify, and redistribute.

---

**Author:** s.a.aris — [GitHub](https://github.com/sa-aris) — solus.aris@proton.me
