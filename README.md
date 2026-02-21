# AAA‑Quality RPG Inventory & Crafting System – v2.2  

**Modern C++17, header‑only, zero third‑party dependencies**  

---

## Table of Contents
1. [Project Overview](#project-overview)  
2. [Why a Home‑Made JSON Parser?](#why-a-home‑made-json-parser)  
3. [Key Features](#key-features)  
4. [Directory Layout](#directory-layout)  
5. [Core Modules](#core-modules)  
   - [result.hpp – `Result<T>` type](#resulthpp---resultt-type)  
   - [json.hpp – Minimal JSON implementation](#jsonhpp---minimal-json-implementation)  
   - [logger.hpp – Thread‑safe logging](#loggerhpp---thread‑safe-logging)  
   - [enums.hpp – Enums & helper functions](#enumshpp---enums--helper-functions)  
   - [item.hpp – Item data model & JSON conversion](#itemhpp---item-data-model--json-conversion)  
   - [item_factory.hpp – Template‑based random item creation](#item_factoryhpp---template‑based-random-item-creation)  
   - [crafting.hpp – Data‑driven recipes](#craftinghpp---data‑driven-recipes)  
   - [inventory.hpp – Inventory management, equipment, crafting integration, persistence](#inventoryhpp---inventory-management-equipment-crafting-integration-persistence)  
   - [main.cpp – Demo command‑line UI](#maincpp---demo-command‑line-ui)  
6. [Building the Project](#building-the-project)  
7. [Running the Demo](#running-the-demo)  
8. [Extending the System](#extending-the-system)  
9. [License](#license)  
10. [Author & Contact](#author--contact)  
11. [Support the Project](#support-the-project)  

---

## Project Overview
This repository implements a **complete RPG inventory and crafting system** suitable for hobby projects, academic assignments, or as a foundation for larger games. The code is written in clean, modern C++17, split into logical header files, and **does not rely on any external libraries**.  

All data (item templates and crafting recipes) are stored as JSON files, parsed by a tiny self‑written JSON library that lives in `src/json.hpp`. The system supports:

* Stacking, weight limits, and slot limits.  
* Equip/unequip logic with dedicated equipment slots.  
* Randomised item generation based on template definitions and rarity scaling.  
* Data‑driven crafting with ingredient validation.  
* Full save/load functionality using JSON.  

A minimal console UI (`src/main.cpp`) demonstrates the workflow and can be replaced with any rendering layer you prefer.

---

## Why a Home‑Made JSON Parser?
* **Portability** – No need to ship a bulky third‑party header (`nlohmann::json` is ~30 kB). The parser fits comfortably in a single header and compiles on every C++17 compiler.  
* **Educational value** – The code shows how a recursive‑descent JSON parser works, which is useful for students and developers learning parsers.  
* **Full control** – Only the features required by the inventory system are exposed (`parse`, `dump`, `operator[]`, `value<T>()`). Unused functionality is omitted, keeping compile times low.  
* **Zero runtime dependencies** – The project can be compiled on constrained platforms (embedded Linux, Windows Subsystem for Linux, macOS, etc.) without extra packages.  

The parser is intentionally **minimal but robust**: it handles objects, arrays, numbers, booleans, null, and proper string escaping.

---

## Key Features
|
 Feature 
|
 Description 
|
|
---------
|
-------------
|
| **Self‑contained** – only the C++17 standard library is needed. |
| **Result\<T\>** – functional‑style error handling without exceptions. |
| **Thread‑safe logger** – console + optional file output (`Log::info/warn/error`). |
| **Rarity system** – stats scale with rarity; colour‑coded output for terminal. |
| **Stackable items** – automatic merging of stacks, custom `maxStack`. |
| **Weight & slot limits** – enforced on every add/remove operation. |
| **Equipment slots** – head, chest, legs, weapon, shield, accessory; auto‑detect based on item ID. |
| **Data‑driven crafting** – recipes loaded from JSON; ingredient checking and consumption. |
| **Persistence** – inventory and equipped gear can be saved to / loaded from a JSON file. |
| **CMake build** – simple `CMakeLists.txt` for cross‑platform compilation. |
| **MIT License** – free to use, modify, and redistribute. |

---

## Directory Layout

. ├─ .gitignore ├─ CMakeLists.txt ├─ README.md ├─ data │ ├─ recipes.json ← crafting recipes │ └─ templates.json ← base item definitions └─ src ├─ crafting.hpp ├─ enums.hpp ├─ inventory.hpp ├─ item.hpp ├─ item_factory.hpp ├─ json.hpp ├─ logger.hpp ├─ main.cpp └─ result.hpp


*All source files are header‑only except `main.cpp`, which contains the demonstration UI.*

---

## Core Modules

### `result.hpp` – `Result<T>` type
* Stores either a value (`std::optional<T>`) or an error string.  
* Implicit conversion to `bool` (`true` = success).  
* Static factories: `Result<T>::ok(value)` and `Result<T>::err(message)`.  
* Specialized for `void` to represent success/failure without a value.  

**Usage pattern**
```cpp
Result<Item> r = factory.create("iron_sword");
if (!r) { std::cerr << r.error(); }
else     { Item sword = r.value(); }

json.hpp

– Minimal JSON implementation

Representation –
std::variant

of null, bool, int64, double, string, array, and object.
Parsing – Recursive‑descent parser (
json::parse

) handling whitespace, escape sequences, numbers, objects, arrays, booleans, and null.
Serialization –
dump(indent)

produces compact or pretty‑printed JSON.
Convenient API –
operator[]

,
at()

,
contains()

,
size()

,
value(key,default)

.
to_json

/
from_json

– free functions used by the generic assignment operator to convert user‑defined types (mirrors the nlohmann‑json approach).
The header is self‑contained; no external symbols are required.

logger.hpp

– Thread‑safe logger

Global
std::mutex

protects console and optional file output.
Log::setFile(path)

opens a log file in append mode.
Log::info

,
Log::warn

,
Log::error

write a level tag (
[Info]

,
[Warn]

,
[Error]

) to both stdout and the file (if opened).
enums.hpp

– Enums & helper functions

enum class ItemType   { Weapon, Armor, Consumable, Material, Misc };
enum class EquipSlot  { Head, Chest, Legs, Weapon, Shield, Accessory, None };
enum class Rarity     { Common, Uncommon, Rare, Epic, Legendary };
enum class Stat       { Attack, Defense, Health, Mana };

Conversion helpers (
toString

,
stringTo*

).
rarityColor()

returns ANSI colour codes for terminal output.
item.hpp

– Item data model & JSON conversion

Payload structs (
WeaponData

,
ArmorData

,
ConsumableData

,
MaterialData

,
MiscData

) each hold the fields relevant to their type (damage, defense, heal amount, weight, …).
ItemPayload

–
std::variant

holding exactly one payload type.
Item

class contains:
id

,
name

,
type

,
rarity

,
levelReq

,
stackSize

,
maxStack

, and the payload.
getWeight()

– total weight of the stack.
weightPerUnit()

– weight of a single item (used when splitting stacks).
getDescription()

– coloured, human‑readable line (e.g., “Epic Iron Sword (x1) – Weapon [DMG:12/120]”).
JSON conversion (
to_json

/
from_json

) enables direct serialization of
Item

objects, vectors of items, and whole inventories.
item_factory.hpp

– Template‑based random item creation

Loads item templates from
data/templates.json

.
create(id, playerLevel)

:
Copies the template.
Adjusts
levelReq

based on the player’s level.
Picks a random rarity using a weighted table (Common 55 %, Uncommon 25 %, Rare 12 %, Epic 6 %, Legendary 2 %).
Multiplies numeric stats by
1.0 + rarityIndex * 0.2

.
Prefixes the name with the rarity text (if applicable).
Sets
maxStack

(20 for consumables/materials, 1 otherwise).
createRandomItem(playerLevel)

picks a random template and forwards to
create

.
All randomness is provided by a
std::mt19937

seeded with
std::random_device

.
crafting.hpp

– Data‑driven recipes

Recipe

struct:
resultId

– ID of the item produced.
resultCount

– quantity produced (default 1).
ingredients

–
std::unordered_map<std::string,int>

mapping required item IDs to quantities.
JSON conversion enables loading a JSON array of recipes from
data/recipes.json

.
CraftingSystem

stores the recipes in an unordered map and provides:
loadFromFile(path)

– parses the file and fills the map.
get(resultId)

– returns a pointer to the recipe or
nullptr

.
has(resultId)

– quick existence test.
inventory.hpp

– Inventory management, equipment, crafting integration, persistence

Construction –
Inventory(slotLimit = 30, weightLimit = 300)

.
Adding items – checks weight and slot availability, merges with existing stacks, updates
totalWeight_

.
Removing items – supports partial stack removal, updates weight.
Counting –
count(id)

returns total quantity of a given item ID.
Equipment handling:
equip(id, playerLevel)

– moves one unit from the inventory to the appropriate
EquipSlot

.
unequip(slot)

– returns the equipped item to the inventory.
Automatic unequip of the currently equipped item (if any) before equipping a new one.
Crafting integration –
craft(resultId, factory, crafting, playerLevel)

validates ingredients, creates the product via
ItemFactory

, checks space with
canAdd()

, consumes ingredients, and stores the result.
Persistence –
serialize()

returns a pretty‑printed JSON string with
items

and
equipment

;
deserialize(jsonString)

restores the state, rebuilding
totalWeight_

and emitting warnings if limits are exceeded.
Utility getters –
getItems()

,
getEquipment()

,
totalWeight()

,
usedSlots()

.
All public functions return
Result<void>

or
Result<Item>

to convey detailed error information.

main.cpp

– Demo command‑line UI

Provides a simple menu that lets the user:

View inventory (with weight/slot usage).
View equipped gear.
Add a random loot item.
Craft an item by entering a recipe ID.
Equip an item by entering its inventory ID.
Unequip a specific slot.
Save the current game state to
savegame.json

.
Load a previously saved game.
The UI demonstrates error handling (
Result

), logging, and the colour‑coded rarity output.

Building the Project

The project uses CMake (minimum required version 3.10). The build process is straightforward:

# Clone the repository
git clone https://github.com/sa-aris/AAA-Inventory-Crafting.git
cd AAA-Inventory-Crafting

# Create a build directory
mkdir build && cd build

# Configure (Release build recommended)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build .

The resulting executable will be placed in
build/

(named
AAA-Inventory-Crafting

on Unix,
AAA-Inventory-Crafting.exe

on Windows).

Dependencies: only a C++17 compiler and CMake. No third‑party libraries are fetched.

Running the Demo

Make sure the data files are reachable (the executable expects
../data/templates.json

and
../data/recipes.json

relative to the working directory). You can either run the program from the
build

folder (keeping the
data

directory at the repository root) or copy the
data

folder next to the executable.

./AAA-Inventory-Crafting   # or .\AAA-Inventory-Crafting.exe on Windows

Follow the on‑screen numeric menu. All actions are logged to
game.log

(created automatically) and to the console.

Extending the System

Area	How to extend
New item type	Add a payload struct,
to_json/from_json

overloads, and a branch in
Item::getDescription

and
slotForItem

.
Additional stats	Extend the payload structs, update the visitor in
Item::getWeight

, and modify the description lambda.
More equip slots	Add a value to
EquipSlot

, update
toString

, and adjust
slotForItem

heuristics.
Complex recipes	Enrich
Recipe

with fields like
requiredLevel

or
craftTime

, then modify
CraftingSystem::loadFromFile

accordingly.
GUI / engine integration	Replace
main.cpp

with your rendering/engine loop; the core headers (
inventory.hpp

,
item_factory.hpp

, etc.) remain unchanged.
Player data persistence	Serialize additional structs (e.g., player stats) alongside the inventory JSON and load them together.
Unit tests	The modular layout makes it trivial to write GoogleTest or Catch2 tests for each header.
Because every component lives in its own header, you can split the
src/

directory into a library (
include/

for public headers) and a binary (
src/

for implementation) without changing any public API.

License

The project is released under the MIT License. See the
LICENSE

file for full terms.

Author & Contact

Name: s.a.aris
E‑mail: s.a.aris@proton.me
GitHub: https://github.com/sa-aris

Feel free to open issues, submit pull requests, or ask questions on the repository.

Support the Project

This project is especially aimed at students, beginners, and developers with limited resources. If you have the means, you can support my ongoing work by buying me a coffee. Anyone who uses my project is, in my eyes, already treating me to a coffee. Therefore, any donation you make will go solely toward enabling more projects like this one. Thank you for your generosity and for sharing the vision!
