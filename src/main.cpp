#include <SDL2/SDL.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <stdexcept> // For std::runtime_error

int main(int argc, char *argv[]) {
  // --- Test SQLiteCpp Database Connection ---
  try {
    // NOTE: Make sure 'initiativ.sqlite' is in your build directory or provide
    // a full path.
    SQLite::Database db("../data/initiativ.sqlite", SQLite::OPEN_READONLY);
    std::cout << "Successfully opened database." << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "SQLite Error: " << e.what() << std::endl;
    return 1; // Exit if DB can't be opened
  }

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

  std::cout << "SDL2 initialized and window created successfully!" << std::endl;

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