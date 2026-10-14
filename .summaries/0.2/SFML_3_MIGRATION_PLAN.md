# SFML 2.6 → SFML 3.1 Migration Plan

## Overview

Current SFML version: **2.6**  
Target SFML version: **3.1.0** (latest stable)

SFML 3.x is a major breaking release. This plan covers all API changes that affect your Blackjack2 project.

---

## Phase 0: Build System Changes ✅ (Already Done)

Updated in this phase:
- **CMakeLists.txt**: Updated `find_package(SFML 3.1 REQUIRED)` and component names
- **CMakePresets.json**: Updated cmakeMinimumRequired reference
- **README.md**: Updated SFML version requirement
- **PROJECT_TECHNICAL_SUMMARY.md**: Updated SFML version reference

No .cpp/.hpp changes yet.

---

## Phase 1: Core Window and View APIs

### Files Affected:
- `Game.hpp`, `Game.cpp`

### Changes Required:

#### 1. sf::VideoMode constructor
**Current (2.6):**
```cpp
sf::VideoMode(width, height)
```

**New (3.1):**
```cpp
sf::VideoMode({width, height})  // uses sf::Vector2u initializer list
```

#### 2. sf::FloatRect constructor
**Current (2.6):**
```cpp
sf::FloatRect(x, y, width, height)
```

**New (3.1):**
```cpp
sf::FloatRect({x, y}, {width, height})  // uses two sf::Vector2f
```

#### 3. sf::View::setSize()
**Current (2.6):**
```cpp
view.setSize(width, height)  // two float params
```

**New (3.1):**
```cpp
view.setSize({width, height})  // sf::Vector2f
```

**Locations in Game.cpp:**
- Constructor: `gameView(sf::FloatRect(...))` → needs new constructor syntax
- Constructor: `window(sf::VideoMode(...))` → needs new constructor syntax
- `resizeView()`: calls to `view.getSize()` still work, but if you set size, use Vector2f

---

## Phase 2: Event System (LARGEST CHANGE)

### Files Affected:
- `Game.cpp` — `processEvents()`

### SFML 3 Event System Overview

SFML 3 replaced the type-tagged `sf::Event` struct with a modern, type-safe system:
- `pollEvent()` now returns an optional-like object (iterates while true)
- No more `event.type` comparison
- Use `event->is<EventType>()` to check event type
- Use `event->getIf<EventType>()` to access event-specific data

### Changes Required:

#### 1. Event loop structure
**Current (2.6):**
```cpp
sf::Event event;
while(window.pollEvent(event))
{
    if(event.type == sf::Event::Closed) { ... }
    if(event.type == sf::Event::KeyPressed) { ... }
}
```

**New (3.1):**
```cpp
while (const auto event = window.pollEvent())
{
    if (event->is<sf::Event::Closed>()) { ... }
    if (auto pressed = event->getIf<sf::Event::KeyPressed>()) { ... }
}
```

#### 2. Event type checks
**Current:**
```cpp
event.type == sf::Event::Closed
event.type == sf::Event::Resized
event.type == sf::Event::KeyPressed
event.type == sf::Event::MouseButtonPressed
event.type == sf::Event::MouseButtonReleased
event.type == sf::Event::MouseMoved
```

**New:**
```cpp
event->is<sf::Event::Closed>()
event->is<sf::Event::Resized>()
event->is<sf::Event::KeyPressed>()
event->is<sf::Event::MouseButtonPressed>()
event->is<sf::Event::MouseButtonReleased>()
event->is<sf::Event::MouseMoved>()
```

#### 3. Accessing keyboard key
**Current:**
```cpp
event.key.code == sf::Keyboard::Up
```

**New:**
```cpp
auto pressed = event->getIf<sf::Event::KeyPressed>();
if (pressed && pressed->code == sf::Keyboard::Key::Up) { ... }
```

#### 4. Mouse button events
**Current:**
```cpp
event.mouseButton.button == sf::Mouse::Left
event.mouseButton.x, event.mouseButton.y
```

**New:**
```cpp
auto pressed = event->getIf<sf::Event::MouseButtonPressed>();
if (pressed && pressed->button == sf::Mouse::Button::Left && pressed->position.x == ...) { ... }
```

#### 5. Mouse move events
**Current:**
```cpp
event.mouseMove.x, event.mouseMove.y
```

**New:**
```cpp
auto moved = event->getIf<sf::Event::MouseMoved>();
if (moved) { moved->position.x, moved->position.y }
```

#### 6. Keyboard enum values
**Current:**
```cpp
sf::Keyboard::Up, sf::Keyboard::Down, sf::Keyboard::Left, sf::Keyboard::Right
sf::Keyboard::H, sf::Keyboard::S, sf::Keyboard::D, sf::Keyboard::P, sf::Keyboard::Y, sf::Keyboard::R, sf::Keyboard::I
```

**New:**
```cpp
sf::Keyboard::Key::Up, sf::Keyboard::Key::Down, sf::Keyboard::Key::Left, sf::Keyboard::Key::Right
sf::Keyboard::Key::H, sf::Keyboard::Key::S, sf::Keyboard::Key::D, sf::Keyboard::Key::P, sf::Keyboard::Key::Y, sf::Keyboard::Key::R, sf::Keyboard::Key::I
```

Note: Mouse button enum also changes:
```cpp
// Current
sf::Mouse::Left
// New
sf::Mouse::Button::Left
```

---

## Phase 3: Keyboard and Mouse Enums

### Files Affected:
- `Game.cpp` — `processEvents()`

### All Keyboard Enums Now Scoped

Every `sf::Keyboard::X` becomes `sf::Keyboard::Key::X`:

| Current | New |
|---------|-----|
| `sf::Keyboard::Up` | `sf::Keyboard::Key::Up` |
| `sf::Keyboard::Down` | `sf::Keyboard::Key::Down` |
| `sf::Keyboard::Left` | `sf::Keyboard::Key::Left` |
| `sf::Keyboard::Right` | `sf::Keyboard::Key::Right` |
| `sf::Keyboard::H` | `sf::Keyboard::Key::H` |
| `sf::Keyboard::S` | `sf::Keyboard::Key::S` |
| `sf::Keyboard::D` | `sf::Keyboard::Key::D` |
| `sf::Keyboard::P` | `sf::Keyboard::Key::P` |
| `sf::Keyboard::Y` | `sf::Keyboard::Key::Y` |
| `sf::Keyboard::R` | `sf::Keyboard::Key::R` |
| `sf::Keyboard::I` | `sf::Keyboard::Key::I` |

### Mouse Button Enum

| Current | New |
|---------|-----|
| `sf::Mouse::Left` | `sf::Mouse::Button::Left` |

---

## Phase 4: Text API Changes

### Files Affected:
- `StatBox.cpp`, `StatBox.hpp`
- `Game.cpp` (drawChips, drawInsurance)
- `HUD.cpp`

### Changes Required:

#### 1. sf::Text construction and font setting
**Current (2.6):**
```cpp
sf::Text text;
text.setFont(font);
text.setCharacterSize(16);
text.setFillColor(sf::Color::White);
text.setString("Hello");
```

**New (3.1):**
```cpp
sf::Text text(font, "Hello", 16.f);
text.setFillColor(sf::Color::White);
```

Note: `setCharacterSize` takes a float now, not int.

#### 2. sf::Text::setColor() → setFillColor()
**Current:**
```cpp
m_title.setColor(sf::Color::White);
```

**New:**
```cpp
m_title.setFillColor(sf::Color::White);
```

### Files and locations:

**StatBox.cpp:**
- `m_title.setFont(font)` → constructor-based init
- `m_title.setColor(...)` → `setFillColor(...)`
- `m_title.setCharacterSize(int)` → float param, constructor-based
- Same for `m_value`

**HUD.cpp:**
- `m_sliderLabel.setFont(...)` → constructor
- `m_controlsText.setFont(...)` → constructor
- Same pattern in multiple places

**Game.cpp:**
- `drawChips()`: creates sf::Text inline
- `drawInsurance()`: creates sf::Text inline

---

## Phase 5: Sprite and Texture API

### Files Affected:
- `Graphics.cpp`, `Graphics.hpp`
- `Game.cpp`

### Changes Required:

#### 1. sf::Sprite constructor
**Current (2.6):**
```cpp
sf::Sprite sprite;
sprite.setTexture(texture);
```

**New (3.1):**
```cpp
sf::Sprite sprite(texture);  // texture passed in constructor
```

Note: In SFML 3, `sf::Sprite` takes a `sf::Texture` by value in constructor (uses shared ownership model). You may need to pass by reference with `sf::shared_from_this`-style or just pass the texture reference directly.

#### 2. sf::Sprite::setTextureRect() → setTextureRect() (may still exist)
**Current:**
```cpp
sprite.setTextureRect(sf::IntRect(x, y, w, h));
```

**New:**
```cpp
sprite.setTextureRect({x, y, w, h})  // or sf::IntRect({x, y}, {w, h})
```

Verify: `sf::IntRect` constructor may have changed similarly to `sf::FloatRect`.

#### 3. sf::RectangleShape position/size
**Current:**
```cpp
shape.setPosition(x, y);
shape.setSize(width, height);
```

**New:**
```cpp
shape.setPosition({x, y});
shape.setSize({width, height});
```

### Locations:

**Graphics.cpp:**
- `makeCardSprite()`: creates sprite with texture, sets texture rect
- `loadCardTexture()`: `texture.loadFromFile()` — verify still works the same

**Game.cpp:**
- `loadAssets()`: `backgroundSprite.setTexture(backgroundTexture)` → constructor init
- `loadAssets()`: `backgroundSprite.setScale(float, float)` → `setScale({x, y})`

**HUD.cpp:**
- `drawRiskSlider()`: creates RectangleShape, CircleShape inline
- Multiple `setPosition(x, y)` and `setSize(w, h)` calls

**Panel.cpp:**
- `setBounds()`: uses `m_background.setPosition(x, y)` and `setSize(w, h)`

---

## Phase 6: Rotation API

### Files Affected:
- `Graphics.cpp` (if any rotation is applied)

### Changes Required:

**Current (2.6):**
```cpp
sprite.setRotation(90.f);  // raw float = degrees
```

**New (3.1):**
```cpp
sprite.setRotation(sf::degrees(90.f));  // strong typed angle
```

Note: Check if your code uses rotation. Currently `Graphics.cpp` and `Game.cpp` don't appear to set rotation on sprites, but double-check.

---

## Phase 7: Rectangle Field Access

### Files Affected:
- `Game.cpp` — `processEvents()` slider handling
- `HUD.cpp` — `drawRiskSlider()`
- `HUD.hpp` — `getRiskSliderBounds()` returns `const sf::FloatRect&`

### Changes Required:

**Current (2.6):**
```cpp
sf::FloatRect rect;
rect.left, rect.top, rect.width, rect.height
```

**New (3.1):**
```cpp
sf::FloatRect rect;
rect.getPosition().x, rect.getPosition().y, rect.getSize().x, rect.getSize().y
// OR
rect.left() if accessor methods exist
```

Note: SFML 3 may have changed from public member variables to accessor methods. This is critical for slider bounds handling.

### Locations with field access:

**Game.cpp (processEvents):**
```cpp
slider.contains(...)  // probably still works
slider.left           // used in slider math: (mousePos.x - slider.left) / slider.width
slider.width          // same line
```

**HUD.cpp (drawRiskSlider):**
```cpp
riskSliderBounds.left, riskSliderBounds.top, riskSliderBounds.width, riskSliderBounds.height
// Used extensively for slider rendering
```

---

## Phase 8: Miscellaneous API Changes

### sf::Vector2u / sf::Vector2f

**Current:**
```cpp
sf::Vector2f(x, y)
sf::Vector2u(w, h)
```

**New:**
```cpp
sf::Vector2f{x, y}  // still works, brace-init preferred
```

This should mostly be compatible, but some constructors may prefer initializer lists.

### sf::Color

Should remain mostly compatible:
- `sf::Color::White`, `sf::Color::Red`, etc. — unchanged
- `sf::Color(r, g, b)` — unchanged

### sf::CircleShape

**Current:**
```cpp
sf::CircleShape(radius)
```

**New:**
Same, but check if radius needs to be float explicitly.

---

## File-by-File Migration Order (Recommended)

### Priority 1: Build Files (✅ DONE)
- CMakeLists.txt
- CMakePresets.json
- README.md
- PROJECT_TECHNICAL_SUMMARY.md

### Priority 2: Window & View Foundation
- `Game.cpp` — constructor changes only (VideoMode, FloatRect)
- `Game.cpp` — `resizeView()` — View API

### Priority 3: Event System
- `Game.cpp` — `processEvents()` — complete rewrite

### Priority 4: Text System
- `StatBox.cpp` — font/character size/color API
- `HUD.cpp` — slider label and controls text
- `Game.cpp` — drawChips() and drawInsurance() text creation

### Priority 5: Sprites & Rendering
- `Graphics.cpp` — sprite construction and texture rect
- `Game.cpp` — loadAssets() sprite setup

### Priority 6: Rectangle Accessors
- `HUD.cpp` — drawRiskSlider() field access
- `Game.cpp` — slider math field access

### Priority 7: Shapes & Positioning
- `HUD.cpp` — drawRiskSlider() RectangleShape/CircleShape
- `Panel.cpp` — setPosition/setSize calls

### Priority 8: Verification & Testing
- Run and verify all visual elements
- Test input handling
- Test slider interaction

---

## Notes

- All game logic (Card, Deck, Hand, Player, etc.) is SFML-agnostic — no changes needed
- Tests (test_card.cpp, test_deck.cpp, test_hand.cpp) are SFML-agnostic — no changes needed
- Serialization and persistence code is SFML-agnostic — no changes needed
- The nlohmann/json embedded library is unaffected

---

## Next Steps

1. Build files are updated — verify SFML 3.1 is available on your system
2. I'll implement changes in the order above, starting with Phase 2 (Game constructor/window/view)
3. After each phase, verify compilation
4. Full migration expected to touch: Game.cpp (heaviest), HUD.cpp, Graphics.cpp, StatBox.cpp, Panel.cpp

</begin_arg>