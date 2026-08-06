# Project Summary: Blackjack C++ Game

## Project Overview
- **Name**: Blackjack2
- **Type**: Single-player Blackjack card game
- **Language**: C++17
- **Graphics Framework**: SFML 2.x
- **Build System**: CMake
- **Version**: 0.1.0

## Architecture

### Entry Point
- **main.cpp**: Creates Game instance, calls run(), handles exceptions

### Core Components

#### Game (Game.hpp/cpp)
- Central controller managing game state and flow
- Contains: SFML window, deck, dealer hand, player, HUD, view
- Manages round states: PlaceBets → OfferInsurance → PlayerTurn → DealerTurn → Cleanup
- Manages game states: Started → LastHand → GameOver
- Key responsibilities:
  - Asset loading (font, textures)
  - Event processing (keyboard input)
  - Game loop (processEvents → update → draw)
  - Card dealing and game logic
  - Basic strategy advisor
  - Card counting (Hi-Lo system)
  - Drawing all game elements

#### Card (Card.hpp/cpp)
- Simple struct: value (1-13) + Suit enum
- Methods: getBlackjackValue(), getBlackjackValueToString(), isAce(), isFaceCard(), isCutCard()
- Blackjack values: 1→11(Ace), 11-13→10(Face), others→face value

#### Deck (Deck.hpp/cpp)
- Multi-deck shoe implementation
- Methods: create(numberOfDecks), shuffle(), draw(), getCard(), empty(), size(), reachedCutCard()
- Cut card system: inserts special card (value=14) at 85% depth, signals shoe end
- Uses mt19937 RNG with random_device seed
- Cards drawn from back of vector

#### Hand (Hand.hpp/cpp)
- Vector of Cards with state flags
- Properties: finished, doubledDown, surrendered, split, currentBet, insuranceBet
- Methods: addCard(), clear(), getValue(), isBlackjack(), isBust(), canSplit(), canDouble()
- Hand value calculation: sums cards, converts aces from 11→1 if bust
- Blackjack: exactly 2 cards totaling 21
- Split allowed: exactly 2 cards with same blackjack value
- Double allowed: exactly 2 cards, not already doubled

#### Player (Player.hpp/cpp)
- Player instance with bankroll (starts at 1000)
- Properties: name, bankroll, hands vector, currentHand index
- Methods: placeBet(), winBet(), loseBet(), pushBet(), resetRound()
- Profile management (partially implemented)
- Note: loseBet() is currently empty - no money deducted on loss!

#### PlayerProfile (PlayerProfile.hpp/cpp)
- Persistence layer using nlohmann/json
- Properties: name, bankroll, stats (PlayerStats), currentHistory, longHistory (ShoeRecord[])
- Methods: load(), save(), addRoundRecord(), addShoeRecord()
- Note: save/load are declared but not fully implemented

#### HUD (HUD.hpp/cpp)
- Heads-up display with 3 panels: left, right, bottom
- Properties: m_showLeft, m_showRight, Panel instances
- Methods: initialize(), update(HUDState), draw(), toggle panels
- Update method mostly commented out - not fully implemented

#### Panel (Panel.hpp/cpp)
- UI container with background rectangle and stat boxes
- Properties: m_background (RectangleShape), m_stats (vector of StatBox*)
- Methods: initialize(), setBounds(), draw()

#### StatBox (StatBox.hpp/cpp)
- Individual display element: title + value text
- Properties: m_title, m_value (SFML Text)
- Methods: initialize(), setValue(), setPosition(), draw()

#### HUDState (HUDState.hpp)
- Data struct passed to HUD for display
- Contains: bankroll, currentBet, streak, runningCount, trueCount, cardsRemaining, statusMessage
- Pointer members for PlayerStats and ShoeRecord (unused)

#### Graphics (Graphics.hpp/cpp)
- Utility namespace for card rendering
- Functions: loadCardTexture(), makeCardSprite(), makeCardBackSprite(), drawCard()
- Sprite sheet layout: x = (value-1)*64, y = suit*64, with padding

#### GameRecord (GameRecord.hpp)
- Data structures for tracking game history
- **PlayerStats**: shoesFinished, handsFinished, wins/losses/pushes/blackjacks/splits/doubles, betsPlaced, betSum, largestHand, greatestWin/Loss, greatestSessionWin/Loss
- **RoundRecord**: dealer hand, player hand, actionSequence, wager, payout, won flag, netProfit()
- **ShoeRecord**: deck, vector of RoundRecords

#### Constants (Constants.hpp)
- Central configuration
- Window: 1400x800
- Card dimensions: 64x64 pixels
- Game rules: 2 decks, dealer stands on 17, max 4 splits, 3:2 blackjack payout
- Betting: min 10, max 500
- Drawing positions: hardcoded floats for dealer, player, deck placement
- **BasicStrategy namespace**: Strategy tables for hard hands, soft hands, pairs, and surrender decisions

#### Serialization (Serialization.hpp)
- Includes nlohmann/json and game types
- JSON serialization code exists as commented-out friend functions in headers
- Not currently active

## Game Flow

1. **Initialization**: Load assets, create multi-deck shoe, shuffle with cut card
2. **PlaceBets**: Player adjusts bet (↑↓) and confirms (Enter)
3. **Deal**: Two cards each to player and dealer, alternating
4. **Insurance** (if dealer shows Ace): Adjust insurance bet (↑↓), confirm (Enter) or decline (Esc)
5. **Blackjack evaluation**: Check for instant blackjacks
6. **PlayerTurn**: Process each hand: Hit(H), Stand(S), Double(D), Split(P), Surrender(R)
7. **DealerTurn**: Dealer draws until 17+, special messages based on final total
8. **EvaluateHands**: Determine winners, update bankroll, record results
9. **Cleanup**: Player presses R for next round
10. **Shoe End**: When cut card reached, finish current hand, save profile, prompt restart

## Known Issues / Incomplete Features

1. **Bankroll bug**: Player::loseBet() is empty - no money deducted when losing hands
2. **HUD incomplete**: update() method mostly commented out, no visual stats display
3. **PlayerProfile**: save() method is empty, load() not fully implemented
4. **Serialization**: JSON code exists but commented out, not functional
5. **Surrender**: Uses R key which conflicts with "Next Round"
6. **Insurance**: DecreaseInsurance() has logic error - prints message after decrement unconditionally
7. **Panel stats**: m_stats pointer never initialized in Panel constructor
8. **Basic strategy**: Some table entries use 'M' which isn't mapped to an action

## Build Configuration

- **CMakeLists.txt**: 
  - Finds SFML 2.5+ (graphics, window, system, audio, network)
  - Links all SFML components
  - Includes from "include" directory
  - Copies assets folder to build output
- **CMakePresets.json**: Build preset configuration (not read)

## Asset Structure

- **assets/fonts/roboto/**: Font file for text rendering
- **assets/images/felt.png**: Table background texture
- **assets/images/cardsLarge_tilemap_packed.png**: Card sprite sheet
  - Grid layout: 14 cards × 4 suits
  - Each card: 64×64 pixels with padding
  - Special card (value 14) used for card back / cut card

## Controls

- **Betting Phase**: ↑ (increase), ↓ (decrease), Enter (confirm)
- **Player Turn**: H (hit), S (stand), D (double down), P (split), R (surrender)
- **Round Management**: R (next round after cleanup, restart after game over)
- **Insurance**: ↑↓ (adjust), Enter (confirm), Esc (decline)

## Card Counting

- Hi-Lo system implemented
- Running count updated on every card dealt:
  - Cards 2-6: +1
  - Cards 10-Ace: -1
  - Cards 7-9: 0
- True count = runningCount / (cardsRemaining / 52)
- Displayed in HUD (when implemented)

## File Organization

```
├── include/          # Header files (declarations)
├── src/              # Implementation files
├── assets/           # Graphics, fonts
├── main.cpp          # Entry point
├── CMakeLists.txt    # Build configuration
└── readme.md         # User documentation
```

## Key Design Decisions

- Single-player against house rules
- Dealer always plays (no player-as-dealer)
- Cut card at 15% from bottom (85% penetration)
- Multiple split hands supported (up to 4)
- Insurance offered when dealer shows Ace
- Surrender allowed on first two cards only
- Basic strategy advisor accessible via 'I' key
- Console output for all actions (for debugging/logging)