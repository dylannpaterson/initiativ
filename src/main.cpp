#include "monster.h" // Include our new monster definition
#include <SDL2/SDL.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Function to fetch all monster names from the database
std::vector<std::string> getMonsterNames(SQLite::Database &db) {
  std::vector<std::string> monsterNames;
  try {
    SQLite::Statement query(db, "SELECT name FROM monsters");
    while (query.executeStep()) {
      monsterNames.push_back(query.getColumn(0));
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterNames: " << e.what() << std::endl;
  }
  return monsterNames;
}

// Function to fetch a single monster's details by name
Monster getMonsterByName(SQLite::Database &db, const std::string &monsterName) {
  Monster monster;
  try {
    // Prepare a query that selects a monster by its name.
    // The '?' is a placeholder for a parameter.
    SQLite::Statement query(
        db, "SELECT name, size, type, alignment, ac, hp, speed, str, dex, con, "
            "intel, wis, cha, cr FROM monsters WHERE name = ?");

    // Bind the monsterName to the placeholder.
    query.bind(1, monsterName);

    // Execute the query. We expect only one result.
    if (query.executeStep()) {
      monster.name = query.getColumn(0).getString();
      monster.size = query.getColumn(1).getString();
      monster.type = query.getColumn(2).getString();
      monster.alignment = query.getColumn(3).getString();
      monster.armorClass = query.getColumn(4).getInt();
      monster.hitPoints = query.getColumn(5).getInt();
      monster.speed = query.getColumn(6).getString();
      monster.strength = query.getColumn(7).getInt();
      monster.dexterity = query.getColumn(8).getInt();
      monster.constitution = query.getColumn(9).getInt();
      monster.intelligence = query.getColumn(10).getInt();
      monster.wisdom = query.getColumn(11).getInt();
      monster.charisma = query.getColumn(12).getInt();
      monster.challengeRating = query.getColumn(13).getString();
    } else {
      std::cerr << "Monster not found: " << monsterName << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterByName: " << e.what() << std::endl;
  }
  return monster;
}

int main(int argc, char *argv[]) {
  // --- Test SQLiteCpp Database Connection ---
  SQLite::Database db("../data/initiativ.sqlite", SQLite::OPEN_READONLY);
  std::cout << "Successfully opened database." << std::endl;

  // --- Fetch and Display a Specific Monster ---
  std::cout << "\n--- Monster Statblock ---" << std::endl;
  Monster aboleth = getMonsterByName(db, "Aboleth");
  aboleth.display();
  std::cout << "-------------------------" << std::endl;

  // --- A Proper SDL2 Initialization and Loop ---
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError()
              << std::endl;
    return 1;
  }

  window =
      SDL_CreateWindow("initiativ - Phase 1 Test", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);

  if (window == nullptr) {
    std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError()
              << std::endl;
    SDL_Quit();
    return 1;
  }

  std::cout << "\nSDL2 initialized and window created successfully!"
            << std::endl;

  // A simple loop to keep the window open for a moment
  SDL_Event e;
  bool quit = false;
  Uint32 startTime = SDL_GetTicks();

  while (!quit && (SDL_GetTicks() - startTime < 3000)) { // Run for 3 seconds
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        quit = true;
      }
    }
  }

  // --- Cleanup ---
  SDL_DestroyWindow(window);
  SDL_Quit();

  std::cout << "\nPhase 1 Foundation Test: Success." << std::endl;
  return 0;
}