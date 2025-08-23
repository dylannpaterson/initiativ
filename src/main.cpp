#include "monster.h" // Include our new monster definition
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h> // We will use this with ImGui
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ImGui Headers
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

// --- Global Variables ---
std::vector<std::string> g_monsterNames;
static int g_selectedMonsterIndex = 0;
static Monster g_currentMonster;
static SQLite::Database *g_db = nullptr;

// --- Function Declarations ---
void renderBestiaryUI();
void renderStatBlock(const Monster &monster);
void initImGui(SDL_Window *window, SDL_GLContext gl_context);
void shutdownImGui();
std::vector<std::string> getMonsterNames(SQLite::Database &db);
Monster getMonsterByName(SQLite::Database &db, const std::string &monsterName);

// --- New Functions to get details from join tables ---
std::vector<std::string> getMonsterSkills(int monsterId, SQLite::Database &db);
std::vector<std::string> getMonsterSavingThrows(int monsterId,
                                                SQLite::Database &db);
std::vector<std::string> getMonsterSenses(int monsterId, SQLite::Database &db);
std::vector<std::string> getMonsterConditionImmunities(int monsterId,
                                                       SQLite::Database &db);
std::vector<std::string> getMonsterDamageImmunities(int monsterId,
                                                    SQLite::Database &db);
std::vector<std::string> getMonsterDamageResistances(int monsterId,
                                                     SQLite::Database &db);
std::vector<std::string> getMonsterDamageVulnerabilities(int monsterId,
                                                         SQLite::Database &db);
std::vector<Ability> getMonsterAbilities(int monsterId, SQLite::Database &db);

// Function to fetch all monster names from the database
std::vector<std::string> getMonsterNames(SQLite::Database &db) {
  std::vector<std::string> monsterNames;
  try {
    SQLite::Statement query(db, "SELECT Name FROM Monsters ORDER BY Name ASC");
    while (query.executeStep()) {
      monsterNames.push_back(query.getColumn(0).getString());
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
    // We must first get the MonsterID to query the join tables
    SQLite::Statement idQuery(db,
                              "SELECT MonsterID FROM Monsters WHERE Name = ?");
    idQuery.bind(1, monsterName);
    int monsterId = -1;
    if (idQuery.executeStep()) {
      monsterId = idQuery.getColumn(0).getInt();
    } else {
      std::cerr << "Monster not found: " << monsterName << std::endl;
      return monster;
    }

    // Now, fetch all core information
    SQLite::Statement coreQuery(
        db, "SELECT Name, Size, Type, Alignment, ArmorClass, HitPoints_Avg, "
            "HitPoints_Formula, Speed, Strength, Dexterity, Constitution, "
            "Intelligence, Wisdom, Charisma, ChallengeRating "
            "FROM Monsters WHERE MonsterID = ?");

    coreQuery.bind(1, monsterId);

    if (coreQuery.executeStep()) {
      monster.name = coreQuery.getColumn(0).getString();
      monster.size = coreQuery.getColumn(1).getString();
      monster.type = coreQuery.getColumn(2).getString();
      monster.alignment = coreQuery.getColumn(3).getString();
      monster.armorClass = coreQuery.getColumn(4).getInt();
      monster.hitPoints = coreQuery.getColumn(5).getInt();
      monster.hitDice = coreQuery.getColumn(6).getString();
      monster.speed = coreQuery.getColumn(7).getString();
      monster.strength = coreQuery.getColumn(8).getInt();
      monster.dexterity = coreQuery.getColumn(9).getInt();
      monster.constitution = coreQuery.getColumn(10).getInt();
      monster.intelligence = coreQuery.getColumn(11).getInt();
      monster.wisdom = coreQuery.getColumn(12).getInt();
      monster.charisma = coreQuery.getColumn(13).getInt();
      monster.challengeRating = coreQuery.getColumn(14).getString();
    }

    // Now, fetch all the additional details from the join tables
    monster.skills = getMonsterSkills(monsterId, db);
    monster.savingThrows = getMonsterSavingThrows(monsterId, db);
    monster.senses = getMonsterSenses(monsterId, db);
    monster.conditionImmunities = getMonsterConditionImmunities(monsterId, db);
    monster.damageImmunities = getMonsterDamageImmunities(monsterId, db);
    monster.damageResistances = getMonsterDamageResistances(monsterId, db);
    monster.damageVulnerabilities =
        getMonsterDamageVulnerabilities(monsterId, db);
    monster.abilities = getMonsterAbilities(monsterId, db);

  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterByName: " << e.what() << std::endl;
  }
  return monster;
}

// --- Implement the new data fetching functions ---
std::vector<std::string> getMonsterSkills(int monsterId, SQLite::Database &db) {
  std::vector<std::string> skills;
  try {
    SQLite::Statement query(
        db, "SELECT Name, Value FROM Skills INNER JOIN Monster_Skills ON "
            "Skills.SkillID = Monster_Skills.SkillID WHERE "
            "Monster_Skills.MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      std::stringstream ss;
      ss << query.getColumn(0).getString() << " +"
         << query.getColumn(1).getInt();
      skills.push_back(ss.str());
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterSkills: " << e.what() << std::endl;
  }
  return skills;
}

std::vector<std::string> getMonsterSavingThrows(int monsterId,
                                                SQLite::Database &db) {
  std::vector<std::string> savingThrows;
  try {
    SQLite::Statement query(
        db,
        "SELECT Name, Value FROM SavingThrows INNER JOIN Monster_SavingThrows "
        "ON SavingThrows.SavingThrowID = Monster_SavingThrows.SavingThrowID "
        "WHERE Monster_SavingThrows.MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      std::stringstream ss;
      ss << query.getColumn(0).getString() << " +"
         << query.getColumn(1).getInt();
      savingThrows.push_back(ss.str());
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterSavingThrows: " << e.what()
              << std::endl;
  }
  return savingThrows;
}

std::vector<std::string> getMonsterSenses(int monsterId, SQLite::Database &db) {
  std::vector<std::string> senses;
  try {
    SQLite::Statement query(
        db, "SELECT Name, Value FROM Senses INNER JOIN Monster_Senses ON "
            "Senses.SenseID = Monster_Senses.SenseID WHERE "
            "Monster_Senses.MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      std::stringstream ss;
      ss << query.getColumn(0).getString() << " "
         << query.getColumn(1).getString();
      senses.push_back(ss.str());
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterSenses: " << e.what() << std::endl;
  }
  return senses;
}

std::vector<std::string> getMonsterConditionImmunities(int monsterId,
                                                       SQLite::Database &db) {
  std::vector<std::string> immunities;
  try {
    SQLite::Statement query(
        db,
        "SELECT Name FROM Conditions INNER JOIN Monster_ConditionImmunities ON "
        "Conditions.ConditionID = Monster_ConditionImmunities.ConditionID "
        "WHERE Monster_ConditionImmunities.MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      immunities.push_back(query.getColumn(0).getString());
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterConditionImmunities: " << e.what()
              << std::endl;
  }
  return immunities;
}

std::vector<std::string> getMonsterDamageImmunities(int monsterId,
                                                    SQLite::Database &db) {
  std::vector<std::string> immunities;
  try {
    SQLite::Statement query(
        db,
        "SELECT Name FROM DamageTypes INNER JOIN Monster_DamageImmunities ON "
        "DamageTypes.DamageTypeID = Monster_DamageImmunities.DamageTypeID "
        "WHERE Monster_DamageImmunities.MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      immunities.push_back(query.getColumn(0).getString());
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterDamageImmunities: " << e.what()
              << std::endl;
  }
  return immunities;
}

std::vector<std::string> getMonsterDamageResistances(int monsterId,
                                                     SQLite::Database &db) {
  std::vector<std::string> resistances;
  try {
    SQLite::Statement query(
        db,
        "SELECT Name FROM DamageTypes INNER JOIN Monster_DamageResistances ON "
        "DamageTypes.DamageTypeID = Monster_DamageResistances.DamageTypeID "
        "WHERE Monster_DamageResistances.MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      resistances.push_back(query.getColumn(0).getString());
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterDamageResistances: " << e.what()
              << std::endl;
  }
  return resistances;
}

std::vector<std::string> getMonsterDamageVulnerabilities(int monsterId,
                                                         SQLite::Database &db) {
  std::vector<std::string> vulnerabilities;
  try {
    SQLite::Statement query(
        db, "SELECT Name FROM DamageTypes INNER JOIN "
            "Monster_DamageVulnerabilities ON DamageTypes.DamageTypeID = "
            "Monster_DamageVulnerabilities.DamageTypeID WHERE "
            "Monster_DamageVulnerabilities.MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      vulnerabilities.push_back(query.getColumn(0).getString());
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterDamageVulnerabilities: " << e.what()
              << std::endl;
  }
  return vulnerabilities;
}

std::vector<Ability> getMonsterAbilities(int monsterId, SQLite::Database &db) {
  std::vector<Ability> abilities;
  try {
    SQLite::Statement query(db, "SELECT Name, Description, AbilityType FROM "
                                "Abilities WHERE MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      Ability ability;
      ability.name = query.getColumn(0).getString();
      ability.description = query.getColumn(1).getString();
      ability.type = query.getColumn(2).getString();
      abilities.push_back(ability);
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterAbilities: " << e.what()
              << std::endl;
  }
  return abilities;
}

int main(int argc, char *argv[]) {
  // --- Initialize SDL2 and OpenGL ---
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    std::cerr << "Error: " << SDL_GetError() << std::endl;
    return -1;
  }

  const char *glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                        SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window *window =
      SDL_CreateWindow("initiativ - Bestiary", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1); // Enable vsync

  // --- Initialize ImGui ---
  initImGui(window, gl_context);

  // --- Test SQLiteCpp Database Connection and Cache Names ---
  static SQLite::Database db("../data/initiativ.sqlite", SQLite::OPEN_READONLY);
  g_db = &db; // A strategic pointer to our database
  std::cout << "Successfully opened database." << std::endl;
  g_monsterNames = getMonsterNames(db);
  std::cout << "Successfully fetched " << g_monsterNames.size()
            << " monster names." << std::endl;

  // Initialize the first monster to display
  if (!g_monsterNames.empty()) {
    g_currentMonster = getMonsterByName(db, g_monsterNames[0]);
  }

  // --- Main application loop ---
  bool done = false;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
        done = true;
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }

    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Render the bestiary UI
    renderBestiaryUI();

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x,
               (int)ImGui::GetIO().DisplaySize.y);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  // --- Cleanup ---
  shutdownImGui();
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  std::cout << "\nPhase 2 Bestiary UI: Success." << std::endl;
  return 0;
}

// --- ImGui Helper Functions ---
void initImGui(SDL_Window *window, SDL_GLContext gl_context) {
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Load a font to improve readability
  io.Fonts->AddFontFromFileTTF("../data/fonts/static/Roboto-Regular.ttf",
                               20.0f);

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init("#version 130");
}

void shutdownImGui() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

// --- New UI Functions ---
void renderStatBlock(const Monster &monster) {
  ImGui::Begin("Monster Statblock", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_MenuBar);

  ImGui::PushStyleColor(
      ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f)); // Gold color for headers
  ImGui::Text("%s", monster.name.c_str());
  ImGui::PopStyleColor();
  ImGui::Text("Size %s, Type %s, Alignment %s", monster.size.c_str(),
              monster.type.c_str(), monster.alignment.c_str());
  ImGui::Separator();

  ImGui::Text("--- Core Stats ---");
  ImGui::Columns(2, "CoreStats", false);

  // Left column
  ImGui::Text("Armor Class: %d", monster.armorClass);
  ImGui::Text("Hit Points: %d (%s)", monster.hitPoints,
              monster.hitDice.c_str());
  ImGui::Text("Speed: %s", monster.speed.c_str());

  // Switch to right column
  ImGui::NextColumn();

  // Right column
  ImGui::Text("STR: %d", monster.strength);
  ImGui::Text("DEX: %d", monster.dexterity);
  ImGui::Text("CON: %d", monster.constitution);
  ImGui::Text("INT: %d", monster.intelligence);
  ImGui::Text("WIS: %d", monster.wisdom);
  ImGui::Text("CHA: %d", monster.charisma);

  ImGui::Columns(1); // Revert to a single column
  ImGui::Separator();

  ImGui::Text("Challenge Rating: %s", monster.challengeRating.c_str());
  if (!monster.savingThrows.empty()) {
    ImGui::Text("Saving Throws: %s",
                [&]() {
                  std::stringstream ss;
                  for (size_t i = 0; i < monster.savingThrows.size(); ++i) {
                    ss << monster.savingThrows[i];
                    if (i < monster.savingThrows.size() - 1) {
                      ss << ", ";
                    }
                  }
                  return ss.str();
                }()
                    .c_str());
  }

  if (!monster.skills.empty()) {
    ImGui::Text("Skills: %s",
                [&]() {
                  std::stringstream ss;
                  for (size_t i = 0; i < monster.skills.size(); ++i) {
                    ss << monster.skills[i];
                    if (i < monster.skills.size() - 1) {
                      ss << ", ";
                    }
                  }
                  return ss.str();
                }()
                    .c_str());
  }

  if (!monster.senses.empty()) {
    ImGui::Text("Senses: %s",
                [&]() {
                  std::stringstream ss;
                  for (size_t i = 0; i < monster.senses.size(); ++i) {
                    ss << monster.senses[i];
                    if (i < monster.senses.size() - 1) {
                      ss << ", ";
                    }
                  }
                  return ss.str();
                }()
                    .c_str());
  }

  if (!monster.damageVulnerabilities.empty()) {
    ImGui::Text("Damage Vulnerabilities: %s",
                [&]() {
                  std::stringstream ss;
                  for (size_t i = 0; i < monster.damageVulnerabilities.size();
                       ++i) {
                    ss << monster.damageVulnerabilities[i];
                    if (i < monster.damageVulnerabilities.size() - 1) {
                      ss << ", ";
                    }
                  }
                  return ss.str();
                }()
                    .c_str());
  }

  if (!monster.damageResistances.empty()) {
    ImGui::Text("Damage Resistances: %s",
                [&]() {
                  std::stringstream ss;
                  for (size_t i = 0; i < monster.damageResistances.size();
                       ++i) {
                    ss << monster.damageResistances[i];
                    if (i < monster.damageResistances.size() - 1) {
                      ss << ", ";
                    }
                  }
                  return ss.str();
                }()
                    .c_str());
  }

  if (!monster.damageImmunities.empty()) {
    ImGui::Text("Damage Immunities: %s",
                [&]() {
                  std::stringstream ss;
                  for (size_t i = 0; i < monster.damageImmunities.size(); ++i) {
                    ss << monster.damageImmunities[i];
                    if (i < monster.damageImmunities.size() - 1) {
                      ss << ", ";
                    }
                  }
                  return ss.str();
                }()
                    .c_str());
  }

  if (!monster.conditionImmunities.empty()) {
    ImGui::Text("Condition Immunities: %s",
                [&]() {
                  std::stringstream ss;
                  for (size_t i = 0; i < monster.conditionImmunities.size();
                       ++i) {
                    ss << monster.conditionImmunities[i];
                    if (i < monster.conditionImmunities.size() - 1) {
                      ss << ", ";
                    }
                  }
                  return ss.str();
                }()
                    .c_str());
  }

  if (!monster.abilities.empty()) {
    ImGui::Separator();
    ImGui::Text("--- Abilities ---");
    for (const auto &ability : monster.abilities) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
      ImGui::Text("%s: %s", ability.type.c_str(), ability.name.c_str());
      ImGui::PopStyleColor();
      ImGui::TextWrapped("%s", ability.description.c_str());
    }
  }

  ImGui::End();
}

void renderBestiaryUI() {
  ImGui::Begin("Bestiary");

  ImGui::Text("Select a monster:");

  // The ListBox takes a function pointer to retrieve names, or an array of
  // char*
  if (ImGui::ListBox(
          "##MonsterList", &g_selectedMonsterIndex,
          [](void *data, int idx) -> const char * {
            std::vector<std::string> *names = (std::vector<std::string> *)data;
            return (*names)[idx].c_str();
          },
          (void *)&g_monsterNames, g_monsterNames.size(),
          20)) { // 20 is the number of visible items
    // This is where the magic happens. When the selection changes, we fetch the
    // new monster.
    if (g_selectedMonsterIndex >= 0 &&
        g_selectedMonsterIndex < g_monsterNames.size()) {
      g_currentMonster =
          getMonsterByName(*g_db, g_monsterNames[g_selectedMonsterIndex]);
    }
  }

  ImGui::End();

  // Display the statblock in a separate window
  if (!g_currentMonster.name.empty()) {
    renderStatBlock(g_currentMonster);
  }
}