# Blackjack2 - C++ Design Principles & Patterns

## Guiding Philosophy

The codebase follows pragmatic C++17 practices. There's no attempt to force advanced patterns where they don't belong. The guiding principles are:

- **Clarity over cleverness** — code should be easy to read and reason about.
- **Encapsulation with discipline** — internals stay private; interfaces stay focused.
- **Separation of concerns** — game logic, rendering, and persistence don't bleed into each other.
- **Modern C++ as default** — use language features that reduce bugs and improve readability.

The project is not an academic exercise in design patterns. It's a working game whose design choices serve correctness, maintainability, and future extensibility.

---

## Core Principles

### 1. Encapsulation and Information Hiding

Each class owns its internal state and exposes a focused interface:

- **Hand** knows how to calculate its value, whether it's bust, soft, or eligible for split/double. Other classes don't reach in and compute that themselves.
- **Deck** manages the cards, the cut card position, and the shuffle. The rest of the program calls `draw()` and `reachedCutCard()` without knowing how those work internally.
- **Player** manages bankroll and betting logic. The Game class doesn't manipulate bankroll directly.

Private members are accessed through well-defined public methods. Where getters/setters exist, they're purposeful—not boilerplate.

This makes refactoring safer and keeps responsibilities localized.

### 2. Const Correctness

The codebase uses `const` aggressively to express intent and prevent accidental mutation:

- Query methods are marked `const`: `Hand::getValue()`, `Deck::size()`, `Player::getBankroll()`
- Parameters that aren't modified are passed by `const&`: `void addCard(const Card& card)`
- References to internal state are exposed carefully: `PlayerStats& getStats()` (mutable, by design), `const Card& getCard()` (read-only access)

This tells future readers (including AI) which functions are safe to call without side effects.

### 3. Value Semantics Where It Makes Sense

Where possible, the code uses value semantics to avoid accidental shared mutation:

- **Card** is a struct with value semantics. It's copied freely—cheap and predictable.
- **HUDState** is a struct passed by value from Game to HUD. HUD reads it without coupling to Game's internals.
- **PlayerStats**, **RoundRecord**, **ShoeRecord** are all structs that can be copied, stored, and serialized.

References and pointers are used where they express ownership or performance intent:
- `PlayerStats& Player::getStats()` — direct mutation of the profile's stats
- `const std::vector<Card>* Deck::getPreviousShoeRecord()` — optional pointer returning nullptr when no record exists

### 4. Single Responsibility

Each class has one reason to change:

| Class | Responsibility |
|-------|----------------|
| **Game** | Orchestration, state machine, input handling |
| **Player** | Bankroll management, hand tracking, profile link |
| **Deck** | Shoe lifecycle: creation, shuffle, draw, cut card |
| **Hand** | Card collection, value calculation, eligibility checks |
| **Card** | Representation of a single card |
| **HUD** | Rendering the UI based on data |
| **PlayerProfile** | Persistence and history storage |
| **Graphics** | Static utilities for drawing cards |

If a class's responsibility list is longer than one bullet, that's a red flag.

### 5. Separation of Logic and Rendering

Game logic never depends on SFML rendering code, and rendering code never depends on game logic. The separation is enforced through architecture:

- **Game** computes state (who has what cards, what the count is, what happened).
- **HUDState** is a plain struct that carries the data the UI needs.
- **HUD** reads HUDState and draws panels, stats, and sliders.
- **Graphics** namespace provides static functions for card rendering.

This means:
- The game logic could be tested or run headlessly without SFML.
- The UI can be redesigned without touching the core game.
- The HUD doesn't need to understand game rules—it just displays numbers and strings.

This is sometimes called **data-driven rendering** or the **entity-view split**, without the naming ceremony.

### 6. State Machines with Strongly-Typed Enums

The game uses `enum class` to model discrete states:

- `RoundState`: `PlaceBets` → `OfferInsurance` → `PlayerTurn` → `DealerTurn` → `Cleanup`
- `GameState`: `Started`, `LastHand`, `GameOver`

Using `enum class` instead of raw `int` or `enum`:
- Prevents accidental comparisons between unrelated enums
- Makes the code self-documenting: `roundState == RoundState::PlayerTurn` is clear
- Enables scoped names: no pollution of the global namespace

The state machine is explicit: input handlers check the current state before acting. This prevents invalid actions (like hitting during the betting phase) and makes the flow traceable.

### 7. Composition Over Inheritance

The project favors composition—building objects from smaller objects—over inheritance hierarchies:

- **Game** has a Deck, a Player, a HUD, a background sprite. It doesn't inherit from any of them.
- **Hand** has a `vector<Card>`. It doesn't inherit from Card or some abstract HandBase.
- **HUD** has Panels and StatBoxes. No inheritance tree.

There are no base classes, no virtual functions, no polymorphic hierarchies. This keeps the code simpler, avoids the pitfalls of deep inheritance, and makes ownership clear.

If future features require polymorphism (e.g., different game variants), it can be introduced where needed without restructuring the entire codebase.

### 8. Structs for Data, Classes for Behavior

The codebase follows a rough convention:

- **Structs** hold data and maybe simple computed methods: `Card`, `PlayerStats`, `HandOutcome`, `RoundRecord`, `ShoeRecord`, `HUDState`. These are primarily value containers.
- **Classes** encapsulate behavior and state: `Game`, `Player`, `Deck`, `Hand`, `HUD`. These manage invariants and operations.

This isn't a hard rule (C++ doesn't distinguish them semantically beyond default access), but it's a useful mental model: structs describe *what*, classes describe *how*.

### 9. Namespaces for Organization

The project uses namespaces to group related functionality:

- `Constants` — all global configuration values in one place. Adding or changing rules is localized.
- `BasicStrategy` — the strategy tables, their enums, and lookup logic, isolated from the rest of the game.
- `Graphics` — static utility functions for rendering cards, decoupled from any particular class.

This avoids global variable pollution and makes the code modular. Future additions (e.g., `Audio`, `UI`, `Networking`) can get their own namespaces.

### 10. Minimal Dependencies

Classes depend on what they need, nothing more:

- **Hand** doesn't know about Game, HUD, or SFML.
- **Card** depends on nothing except the standard library.
- **Deck** depends only on Card.
- **Player** depends on Hand and Deck (for types), but not on Game.
- **HUD** depends on SFML (rendering) and its own UI components, but not on game logic.

This makes it easier to:
- Move files around
- Test components in isolation
- Replace or refactor individual parts

The dependency graph is roughly tree-shaped, not tangled.

### 11. Modern C++ Features Used Deliberately

The project uses C++17 features where they improve safety or clarity:

- `enum class` — type-safe enums for states and actions
- `constexpr` — compile-time constants (window size, card dimensions, game rules)
- `auto` — reduces noise where type is obvious, but not overused
- Range-based for loops — cleaner iteration over containers
- `[[nodiscard]]` — marks functions whose return value should not be ignored (e.g., `Deck::draw()`)
- Structured initialization — `Card{value, suit}` instead of verbose constructors
- `std::optional`-style patterns — returning `nullptr` from pointer-returning functions when a value might not exist

These features reduce boilerplate and prevent entire classes of bugs, without making the code feel like a language tour.

### 12. Inline Functions for Performance and Convenience

Small functions that are called frequently are inlined in headers:

- `RoundRecord::totalWager()`, `totalPayout()`, `netProfit()` — computed on demand, no extra storage
- Serialization functions (`to_json`, `from_json`) — inlined so they're available at compile time
- Constants and utility functions in `Constants.hpp` — `constexpr` and inline

This keeps hot paths fast and reduces function call overhead, without sacrificing readability.

### 13. Defensive Programming

The code checks for invalid states and edge cases:

- `Hand::canSplit()` returns false if there aren't exactly two cards
- `Deck::draw()` handles the cut card internally instead of letting it leak into game logic
- `Player::placeBet()` returns `false` if the bet exceeds bankroll
- Boundary checks on the Kelly slider (clamped to [0.0, 1.0])

These checks prevent crashes and make the behavior predictable. The game doesn't silently accept bad input.

### 14. Single Source of Truth

Each piece of data has one authoritative owner:

- Bankroll is owned by `Player` and synchronized with `PlayerProfile`.
- The deck state is owned by `Deck`. Game reads from it but doesn't modify it directly.
- Round and shoe records are built by `Game::evaluateHands()` and stored in `PlayerProfile`.
- Constants are defined once in `Constants.hpp` and included everywhere.

When data needs to be shared, it's either:
- Passed by const reference (read-only access)
- Passed by reference (controlled mutation)
- Copied (independent snapshot)

This prevents subtle bugs from desynchronized state.

### 15. Persistence as a Concern

Saving and loading is isolated in `PlayerProfile`:

- `PlayerProfile::load()` and `PlayerProfile::save()` handle all file I/O.
- Serialization is handled by `to_json`/`from_json` functions in `Serialization.hpp`.
- The rest of the game doesn't know about JSON or file paths—it just calls `saveProfile()` and `getProfile()`.

This makes it easy to:
- Change the save format without touching game logic
- Add migration logic for older saves
- Swap JSON for another format if needed

---

## Patterns Observed

### Singleton (FontManager)

`FontManager` is a minimal singleton for loading the game font once:

```cpp
static bool load(const std::string& path);
static sf::Font& get();
```

This is a practical choice: fonts are expensive to load, and only one is needed. It's not over-engineered—just a static pair of functions.

### Data Transfer Object (HUDState)

`HUDState` is a plain struct that carries data from Game to HUD. It has no behavior, just fields. This decouples the two and makes the data flow explicit.

### Builder/Creator (Deck)

`Deck::create()` and `Deck::shuffle()` set up the shoe. The constructor is default; the deck is built in steps. This gives flexibility (different number of decks, different cut positions) without overloading the constructor.

### State Object (RoundRecord, ShoeRecord)

Each completed round and shoe is captured as a data object. These are immutable snapshots that can be stored, replayed, or analyzed. This is a simple but powerful pattern for any game that cares about history.

---

## What's Deliberately Avoided

The codebase consciously avoids several things:

- **Deep inheritance hierarchies** — composition is preferred.
- **Heavy templating** — used only where the standard library requires it.
- **Smart pointer churn** — ownership is clear enough with raw references and value semantics.
- **Over-abstraction** — no "GameFactory", "CardRepository", or "HandStrategy". Real names, real responsibilities.
- **Global state** — everything is owned by a class or namespaced. No free-floating variables.

The goal is to keep the codebase understandable by someone reading it for the first time—human or AI.

---

## Design for Future Work

The current design supports the project's evolution without major refactoring:

- **Rule variants**: Constants and strategy tables are centralized. New rule sets can be added as parallel configurations without changing core logic.
- **Replay system**: `ShoeRecord` and `reconstructDeckAtRound()` are already in place. A replay renderer can be added without modifying game logic.
- **Daily seed competition**: `Deck::shuffle()` uses `std::mt19937`. Seeding it with a daily value is a one-line change. Results can be exported from existing record structures.
- **Multi-profile support**: `PlayerProfile::load()` is already file-based. Supporting multiple profiles is a matter of adding profile selection logic.
- **Networking or shared features**: The clean separation between logic and rendering means the game loop could be adapted for remote play without rewriting the core.

The architecture is designed to be extended incrementally, not replaced wholesale.

---

## Summary

Blackjack2's C++ design is grounded in practical principles:

- Encapsulate state, expose focused interfaces
- Use `const` and value semantics to prevent bugs
- Separate logic from rendering and persistence
- Prefer composition over inheritance
- Use modern C++ features where they help
- Keep dependencies minimal and explicit
- Design for clarity and future extension

The result is a codebase that's easy to understand, safe to modify, and ready for the next phase of the project—whether that's replay, competition, rule variants, or something else entirely.