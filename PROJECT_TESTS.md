# Blackjack2 Tests

Automated tests for the Blackjack2 game logic.

## Prerequisites

- **CMake 3.10 or higher** - Used to configure and build the project.
- **GCC or Clang** - A C++ compiler that supports C++17.
- **Git** - Required so CMake can download the Catch2 testing framework automatically.

That's it. You don't need to install Catch2 manually — it is fetched automatically when you configure the project.

## Running the Tests

1. Configure the project:
   ```bash
   cmake --preset "GCC 13.3.0 x86_64-linux-gnu"
   ```

2. Build the test executable:
   ```bash
   cmake --build "out/build/GCC 13.3.0 x86_64-linux-gnu" --target Blackjack2Tests
   ```

3. Run the tests:

   - Directly:
     ```bash
     cd "out/build/GCC 13.3.0 x86_64-linux-gnu"
     ./Blackjack2Tests
     ```

   - Or via ctest:
     ```bash
     cd "out/build/GCC 13.3.0 x86_64-linux-gnu"
     ctest --output-on-failure
     ```

A successful run will end with a message like:
```
All tests passed (44 assertions in 14 test cases)
```

## Test List

### Card Tests (`tests/test_card.cpp`)

| Test Case | Description |
|-----------|-------------|
| `Card values are calculated correctly` | Checks that regular cards return their face value, face cards (J/Q/K) return 10, and Ace returns 11. |
| `Card identification functions work` | Verifies `isAce()` and `isFaceCard()` correctly identify Ace and face cards. |
| `Card value strings are correct` | Checks that `getBlackjackValueToString()` returns "Ace", "Face", or the numeric value as appropriate. |

### Deck Tests (`tests/test_deck.cpp`)

| Test Case | Description |
|-----------|-------------|
| `Deck is empty when created` | Confirms a new Deck starts with 0 cards. |
| `Deck creates correct number of cards` | Checks that `create(1)` produces 52 cards and `create(2)` produces 104 cards. |
| `Drawing from deck works` | Ensures `draw()` returns a valid card and reduces the deck size by 1. |
| `Deck becomes empty after drawing all cards` | Confirms the deck reports as empty after all cards are drawn. |

### Hand Tests (`tests/test_hand.cpp`)

| Test Case | Description |
|-----------|-------------|
| `Empty hand has value zero` | Confirms a new Hand has value 0 and size 0. |
| `Simple hand values are correct` | Checks that a hand with 5 + 7 correctly reports value 12. |
| `Ace counting works correctly` | Verifies that a soft hand (A + 7) counts Ace as 11, but after adding another card, the Ace converts to 1 and the hand becomes hard. |
| `Blackjack is detected correctly` | Confirms that an Ace + face card is detected as blackjack, and that 3+ cards totaling 21 is not blackjack. |
| `Bust is detected correctly` | Checks that a hand over 21 is flagged as bust. |
| `Hand clear works` | Ensures `clear()` resets the hand's cards, value, and state. |
| `Hand split rules are correct` | Verifies that a hand with two equal-value cards can be split, but different-value cards cannot. |

## Adding New Tests

To add more tests:

1. Open the appropriate test file in `tests/` (or create a new one, e.g., `tests/test_game.cpp`).
2. Add a `TEST_CASE` block:

   ```cpp
   TEST_CASE("Short description of what this tests", "[Tag]")
   {
       // Arrange: set up objects
       Hand hand;
       Card card;
       card.value = 5;
       hand.addCard(card);

       // Assert: check behavior
       CHECK(hand.getValue() == 5);
   }
   ```

3. If you created a new test file, add it to `CMakeLists.txt` under `target_sources(Blackjack2Tests PRIVATE ...)`.
4. Rebuild and run the tests as described above.