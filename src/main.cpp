#include "monster.h" // Include our new monster definition
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h> // We will use this with ImGui
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm> // For std::transform
#include <algorithm> // For std::sort
#include <cctype>    // For ::tolower
#include <ctime>     // To seed the winds of chance
#include <iostream>
#include <random> // For the casting of lots
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
static char g_searchBuffer[256] = ""; // Buffer for the search input
static std::vector<std::string>
    g_filteredMonsterNames; // To hold the filtered names
static std::vector<Combatant>
    g_encounterList; // Our assembled forces are now Combatants
static char g_newPlayerNameBuffer[256] = ""; // Buffer for the new player's name
static int g_newPlayerInitiative = 0; // Buffer for the new player's initiative
static int g_currentTurnIndex = -1;   // -1 indicates combat has not begun
static bool g_combatHasBegun = false; // Is the battle joined?

// --- Combat Log ---
struct LogEntry {
  std::string message;
};
std::vector<LogEntry> g_combatLog;

// --- Function Declarations ---
void renderBestiaryUI();
void renderCombatUI();    // The new battlefield view
void renderEncounterUI(); // Our new command tent
void renderCombatLogUI();
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
std::vector<Ability>
getMonsterAbilities(int monsterId,
                    SQLite::Database &db); // Corrected declaration
std::vector<std::string> getMonsterSpeeds(int monsterId, SQLite::Database &db);
std::vector<int> getMonsterSpellSlots(int monsterId, SQLite::Database &db);

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
std::vector<int> getMonsterSpellSlots(int monsterId, SQLite::Database &db) {
  std::vector<int> spellSlots(9, 0);
  try {
    SQLite::Statement query(
        db,
        "SELECT SpellLevel, Slots FROM Monster_SpellSlots WHERE MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      int level = query.getColumn(0).getInt();
      int slots = query.getColumn(1).getInt();
      if (level >= 1 && level <= 9) {
        spellSlots[level - 1] = slots;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterSpellSlots: " << e.what()
              << std::endl;
  }
  return spellSlots;
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
            "HitPoints_Formula, Strength, Dexterity, Constitution, "
            "Intelligence, Wisdom, Charisma, ChallengeRating, Languages "
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
      monster.strength = coreQuery.getColumn(7).getInt();
      monster.dexterity = coreQuery.getColumn(8).getInt();
      monster.constitution = coreQuery.getColumn(9).getInt();
      monster.intelligence = coreQuery.getColumn(10).getInt();
      monster.wisdom = coreQuery.getColumn(11).getInt();
      monster.charisma = coreQuery.getColumn(12).getInt();
      monster.challengeRating = coreQuery.getColumn(13).getString();
      monster.languages = coreQuery.getColumn(14).getString();
    }

    // Now, fetch all the additional details from the join tables
    monster.speeds = getMonsterSpeeds(monsterId, db);
    monster.skills = getMonsterSkills(monsterId, db);
    monster.savingThrows = getMonsterSavingThrows(monsterId, db);
    monster.senses = getMonsterSenses(monsterId, db);
    monster.conditionImmunities = getMonsterConditionImmunities(monsterId, db);
    monster.damageImmunities = getMonsterDamageImmunities(monsterId, db);
    monster.damageResistances = getMonsterDamageResistances(monsterId, db);
    monster.damageVulnerabilities =
        getMonsterDamageVulnerabilities(monsterId, db);
    monster.abilities = getMonsterAbilities(monsterId, db);
    monster.spellSlots = getMonsterSpellSlots(monsterId, db);

  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterByName: " << e.what() << std::endl;
  }
  return monster;
}

// --- Implement the new data fetching functions ---
std::vector<std::string> getMonsterSpeeds(int monsterId, SQLite::Database &db) {
  std::vector<std::string> speeds;
  try {
    SQLite::Statement query(
        db, "SELECT SpeedType, Value FROM Monster_Speeds WHERE MonsterID = ?");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      std::stringstream ss;
      ss << query.getColumn(0).getString() << " "
         << query.getColumn(1).getString();
      speeds.push_back(ss.str());
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterSpeeds: " << e.what() << std::endl;
  }
  return speeds;
}

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

int main(int argc, char *argv[]) {
  // --- Initialize SDL2 and OpenGL ---
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    std::cerr << "Error: " << SDL_GetError() << std::endl;
    return -1;
  }

  // Seed the random number generator once at the start of the campaign
  srand(time(nullptr));

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

  // Initialize the UI with the full list and select the first monster
  g_filteredMonsterNames = g_monsterNames;
  if (!g_filteredMonsterNames.empty()) {
    g_currentMonster = getMonsterByName(db, g_filteredMonsterNames[0]);
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

    // --- Conditional Rendering based on Combat State ---
    if (g_combatHasBegun) {
      renderEncounterUI(); // The roster is always visible
      renderCombatUI();    // The new combat console
    } else {
      renderBestiaryUI(); // Show setup tools
      renderEncounterUI();
      if (!g_currentMonster.name.empty()) {
        renderStatBlock(g_currentMonster); // Show statblock during setup
      }
    }

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
  (void)io; // Suppress warning about unused variable
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Load a font to improve readability
  io.Fonts->AddFontFromFileTTF("../data/fonts/static/Roboto-Regular.ttf",
                               36.0f);

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init("#version 130");
}

void shutdownImGui() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

// Helper function to render a list of strings
void renderStringList(const char *label, const std::vector<std::string> &list) {
  if (!list.empty()) {
    ImGui::Text("%s", label);
    ImGui::SameLine();
    std::stringstream ss;
    for (size_t i = 0; i < list.size(); ++i) {
      ss << list[i];
      if (i < list.size() - 1) {
        ss << ", ";
      }
    }
    ImGui::TextWrapped("%s", ss.str().c_str());
  }
}

// Helper function to create a labeled field
void renderLabeledField(const char *label, const char *value) {
  ImGui::Text("%s", label);
  ImGui::SameLine();
  ImGui::Text("%s", value);
}

// Helper function to calculate a modifier from a stat score
int calculateModifier(int score) { return (score - 10) / 2; }

void renderStatBlock(const Monster &monster) {
  ImGui::SetNextWindowSize(ImVec2(500, 700), ImGuiCond_FirstUseEver);
  ImGui::Begin("Monster Statblock", nullptr, ImGuiWindowFlags_MenuBar);

  // Header section
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
  ImGui::Text("%s", monster.name.c_str());
  ImGui::PopStyleColor();
  ImGui::Text("Size %s, Type %s, Alignment %s", monster.size.c_str(),
              monster.type.c_str(), monster.alignment.c_str());
  ImGui::Separator();

  // Core Stats
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
  if (ImGui::CollapsingHeader("Core Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    if (ImGui::BeginTable("CoreStatsTable", 2,
                          ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      renderLabeledField("Armor Class:",
                         std::to_string(monster.armorClass).c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("Challenge Rating: %s", monster.challengeRating.c_str());
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Hit Points: %d (%s)", monster.hitPoints,
                  monster.hitDice.c_str());
      ImGui::TableSetColumnIndex(1);
      renderStringList("Speeds:", monster.speeds);
      ImGui::EndTable();
    }
    ImGui::PopStyleColor();
  }
  ImGui::PopStyleColor();

  ImGui::Separator();

  // The six core attributes in a clean, horizontal format
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
  if (ImGui::CollapsingHeader("Attributes", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    if (ImGui::BeginTable("AttributeTable", 6,
                          ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_NoHostExtendX)) {
      ImGui::TableSetupColumn("STR");
      ImGui::TableSetupColumn("DEX");
      ImGui::TableSetupColumn("CON");
      ImGui::TableSetupColumn("INT");
      ImGui::TableSetupColumn("WIS");
      ImGui::TableSetupColumn("CHA");
      ImGui::TableHeadersRow();
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%d (%s%d)", monster.strength,
                  calculateModifier(monster.strength) >= 0 ? "+" : "",
                  calculateModifier(monster.strength));
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d (%s%d)", monster.dexterity,
                  calculateModifier(monster.dexterity) >= 0 ? "+" : "",
                  calculateModifier(monster.dexterity));
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d (%s%d)", monster.constitution,
                  calculateModifier(monster.constitution) >= 0 ? "+" : "",
                  calculateModifier(monster.constitution));
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d (%s%d)", monster.intelligence,
                  calculateModifier(monster.intelligence) >= 0 ? "+" : "",
                  calculateModifier(monster.intelligence));
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%d (%s%d)", monster.wisdom,
                  calculateModifier(monster.wisdom) >= 0 ? "+" : "",
                  calculateModifier(monster.wisdom));
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%d (%s%d)", monster.charisma,
                  calculateModifier(monster.charisma) >= 0 ? "+" : "",
                  calculateModifier(monster.charisma));
      ImGui::EndTable();
    }
    ImGui::PopStyleColor();
  }
  ImGui::PopStyleColor();

  ImGui::Separator();

  // --- Tactical Change: Consolidated and Corrected Information Section ---
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
  if (ImGui::CollapsingHeader("Additional Information")) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

    renderStringList("Saving Throws:", monster.savingThrows);
    renderStringList("Skills:", monster.skills);
    renderStringList("Damage Vulnerabilities:", monster.damageVulnerabilities);
    renderStringList("Damage Resistances:", monster.damageResistances);
    renderStringList("Damage Immunities:", monster.damageImmunities);
    renderStringList("Condition Immunities:", monster.conditionImmunities);

    // Senses and Languages are now rendered correctly and only once here.
    renderStringList("Senses:", monster.senses);
    if (!monster.languages.empty()) {
      renderLabeledField("Languages:", monster.languages.c_str());
    }

    ImGui::PopStyleColor();
  }
  ImGui::PopStyleColor();

  // Abilities Section
  if (!monster.abilities.empty()) {
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
    if (ImGui::CollapsingHeader("Abilities")) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
      for (const auto &ability : monster.abilities) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
        ImGui::Text("%s: %s", ability.type.c_str(), ability.name.c_str());
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s", ability.description.c_str());
        ImGui::Separator();
      }
      ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor();
  }

  ImGui::End();
}

void renderBestiaryUI() {
  ImGui::Begin("Bestiary");

  ImGui::Text("Select a monster:");

  // A search bar to filter the list. If the text changes, we reset the
  // selection.
  if (ImGui::InputText("Search", g_searchBuffer,
                       IM_ARRAYSIZE(g_searchBuffer))) {
    g_selectedMonsterIndex = 0; // Reset selection when filter changes
  }

  // Filter the master list of monster names into our temporary list
  std::string filter = g_searchBuffer;
  std::transform(filter.begin(), filter.end(), filter.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  g_filteredMonsterNames.clear();
  if (filter.empty()) {
    // If the search is empty, show the full list
    g_filteredMonsterNames = g_monsterNames;
  } else {
    for (const auto &name : g_monsterNames) {
      std::string lower_name = name;
      std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                     [](unsigned char c) { return std::tolower(c); });

      if (lower_name.find(filter) != std::string::npos) {
        g_filteredMonsterNames.push_back(name);
      }
    }
  }

  // The ListBox now operates on our filtered list of names
  if (ImGui::ListBox(
          "##MonsterList", &g_selectedMonsterIndex,
          [](void *data, int idx) -> const char * {
            auto *names = static_cast<std::vector<std::string> *>(data);
            if (idx >= 0 && idx < names->size()) {
              return (*names)[idx].c_str();
            }
            return ""; // Should be an impossible state
          },
          (void *)&g_filteredMonsterNames, g_filteredMonsterNames.size(),
          20)) { // 20 is the number of visible items
    // When a selection is made, fetch the monster by its name from the filtered
    // list
    if (g_selectedMonsterIndex >= 0 &&
        g_selectedMonsterIndex < g_filteredMonsterNames.size()) {
      g_currentMonster = getMonsterByName(
          *g_db, g_filteredMonsterNames[g_selectedMonsterIndex]);
    }
  }

  ImGui::Separator(); // A clean division

  // The button to add the selected monster to our forces
  if (!g_filteredMonsterNames.empty() && g_selectedMonsterIndex >= 0 &&
      g_selectedMonsterIndex < g_filteredMonsterNames.size()) {
    if (ImGui::Button("Add to Encounter")) {
      // Create a new combatant from the selected monster
      Combatant newCombatant(g_currentMonster);

      // Strategically assign a unique name (e.g., Orc 1, Orc 2)
      int count = 0;
      for (const auto &combatant : g_encounterList) {
        if (combatant.base.name == newCombatant.base.name) {
          count++;
        }
      }
      if (count > 0) {
        newCombatant.displayName =
            newCombatant.base.name + " " + std::to_string(count + 1);
      }

      g_encounterList.push_back(newCombatant);
    }
  }

  ImGui::End();
}

void renderEncounterUI() {
  ImGui::Begin("Encounter");

  // --- Section to Add Player Characters ---
  ImGui::SeparatorText("Party");

  ImGui::PushItemWidth(150);
  ImGui::InputText("Player Name", g_newPlayerNameBuffer,
                   IM_ARRAYSIZE(g_newPlayerNameBuffer));
  ImGui::PopItemWidth();

  ImGui::SameLine();

  ImGui::PushItemWidth(80);
  ImGui::InputInt("Initiative", &g_newPlayerInitiative, 0, 0,
                  ImGuiInputTextFlags_CharsDecimal);
  ImGui::PopItemWidth();

  ImGui::SameLine();

  if (ImGui::Button("Add Player")) {
    if (strlen(g_newPlayerNameBuffer) > 0) {
      Combatant newPlayer;
      newPlayer.isPlayer = true;
      newPlayer.displayName = g_newPlayerNameBuffer;
      newPlayer.initiative = g_newPlayerInitiative;
      newPlayer.currentHitPoints = 0; // Not tracked
      newPlayer.maxHitPoints = 0;     // Not tracked
      g_encounterList.push_back(newPlayer);

      g_newPlayerNameBuffer[0] = '\0';
      g_newPlayerInitiative = 0;
    }
  }

  ImGui::SeparatorText("Combatants");

  if (!g_encounterList.empty()) {
    if (!g_combatHasBegun) {
      if (ImGui::Button("Begin Combat")) {
        for (auto &combatant : g_encounterList) {
          if (!combatant.isPlayer) {
            int modifier = calculateModifier(combatant.base.dexterity);
            combatant.initiative = (rand() % 20 + 1) + modifier;
          }
        }
        std::sort(g_encounterList.begin(), g_encounterList.end(),
                  [](const Combatant &a, const Combatant &b) {
                    return a.initiative > b.initiative;
                  });
        g_currentTurnIndex = 0;
        g_combatHasBegun = true; // Raise the banner!
      }
    } else {
      if (ImGui::Button("End Combat")) {
        g_currentTurnIndex = -1;
        g_combatHasBegun = false; // Lower the banner
      }
    }

    // "Next/Previous Turn" buttons should only be active during combat
    if (g_combatHasBegun) {
      ImGui::SameLine();
      if (ImGui::Button("Next Turn")) {
        if (g_currentTurnIndex != -1) {
          g_currentTurnIndex =
              (g_currentTurnIndex + 1) % g_encounterList.size();
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Previous Turn")) {
        if (g_currentTurnIndex != -1) {
          g_currentTurnIndex =
              (g_currentTurnIndex - 1 + g_encounterList.size()) %
              g_encounterList.size();
        }
      }
    }
  }

  ImGui::Spacing();

  if (g_encounterList.empty()) {
    ImGui::Text("No combatants have been added yet.");
  } else {
    if (ImGui::BeginTable("EncounterTable", 4, ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 200.0f);
      ImGui::TableSetupColumn("Initiative", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableHeadersRow();

      int combatant_to_remove = -1;
      for (int i = 0; i < g_encounterList.size(); ++i) {
        ImGui::PushID(i);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        bool is_current_turn = (i == g_currentTurnIndex);
        if (is_current_turn) {
          ImGui::PushStyleColor(ImGuiCol_Header,
                              ImVec4(0.9f, 0.6f, 0.0f, 1.0f));
        }
        std::string label = g_encounterList[i].displayName + " (" +
                            std::to_string(g_encounterList[i].initiative) +
                            ")";
        if (ImGui::Selectable(label.c_str(), is_current_turn)) {
          g_currentTurnIndex = i;
        }
        if (is_current_turn) {
          ImGui::PopStyleColor();
        }

        ImGui::TableSetColumnIndex(1);
        if (g_encounterList[i].isPlayer) {
          ImGui::Text("Player");
        } else {
          bool is_dead = (g_encounterList[i].currentHitPoints <= 0);
          if (is_dead) {
            ImGui::BeginDisabled();
          }
          if (ImGui::Button("-")) {
            g_encounterList[i].currentHitPoints--;
          }
          if (is_dead) {
            ImGui::EndDisabled();
          }

          ImGui::SameLine();
          ImGui::Text("%d/%d", g_encounterList[i].currentHitPoints,
                      g_encounterList[i].maxHitPoints);
          ImGui::SameLine();

          bool at_max_hp = (g_encounterList[i].currentHitPoints >=
                            g_encounterList[i].maxHitPoints);
          if (at_max_hp) {
            ImGui::BeginDisabled();
          }
          if (ImGui::Button("+")) {
            g_encounterList[i].currentHitPoints++;
          }
          if (at_max_hp) {
            ImGui::EndDisabled();
          }
        }

        ImGui::TableSetColumnIndex(2);
        ImGui::InputInt("##Initiative", &g_encounterList[i].initiative);

        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button("Remove")) {
          combatant_to_remove = i;
        }

        ImGui::PopID();
      }

      if (combatant_to_remove != -1) {
        if (combatant_to_remove == g_currentTurnIndex) {
          g_currentTurnIndex = -1;
        }
        g_encounterList.erase(g_encounterList.begin() + combatant_to_remove);
      }
      ImGui::EndTable();
    }
  }

  ImGui::End();
}
// --- Renders the dedicated UI for an active combat encounter ---
void renderCombatUI() {
  if (!g_combatHasBegun || g_currentTurnIndex < 0 ||
      g_currentTurnIndex >= g_encounterList.size()) {
    return;
  }

  ImGui::Begin("Combat Operations");

  Combatant &activeCombatant = g_encounterList[g_currentTurnIndex];

  ImGui::Text("Current Turn: ");
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
  ImGui::Text("%s", activeCombatant.displayName.c_str());
  ImGui::PopStyleColor();
  ImGui::Separator();

  if (!activeCombatant.isPlayer) {
    ImGui::SeparatorText("Abilities");
    if (activeCombatant.base.abilities.empty()) {
      ImGui::Text("This creature has no special abilities.");
    } else {
      auto &usesMap = activeCombatant.abilityUses;
      for (const auto &ability : activeCombatant.base.abilities) {
        ImGui::PushID(&ability);

        bool is_limited_by_uses = (ability.usesMax > 0);
        int remaining_uses = is_limited_by_uses ? usesMap[ability.name] : 0;

        if (is_limited_by_uses && remaining_uses <= 0) {
          ImGui::PushStyleColor(
              ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        }

        ImGui::Text("[%s] %s", ability.type.c_str(), ability.name.c_str());
        ImGui::TextWrapped("%s", ability.description.c_str());

        // --- Display usage information ---
        if (is_limited_by_uses) {
          ImGui::SameLine();
          if (remaining_uses <= 0)
            ImGui::BeginDisabled();
          if (ImGui::Button("Use")) {
            usesMap[ability.name]--;
          }
          if (remaining_uses <= 0)
            ImGui::EndDisabled();

          ImGui::Text("Uses remaining: %d/%d", remaining_uses, ability.usesMax);
        } else if (ability.rechargeValue > 0) {
          ImGui::Text("Usage: Recharge on a roll of %d-6",
                      ability.rechargeValue);
        }

        if (is_limited_by_uses && remaining_uses <= 0) {
          ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::PopID();
      }
    }

    // --- Spell Slot Tracking ---
    bool is_spellcaster = false;
    for (const auto &slot : activeCombatant.spellSlots) {
      if (slot > 0) {
        is_spellcaster = true;
        break;
      }
    }

    if (is_spellcaster) {
      ImGui::SeparatorText("Spell Slots");
      if (ImGui::BeginTable("SpellSlotsTable", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Slots", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < activeCombatant.spellSlots.size(); ++i) {
          if (activeCombatant.maxSpellSlots[i] > 0) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Level %d", i + 1);
            ImGui::TableSetColumnIndex(1);
            if (ImGui::InputInt(("##level" + std::to_string(i)).c_str(),
                               &activeCombatant.spellSlots[i])) {
              if (activeCombatant.spellSlots[i] < 0) {
                activeCombatant.spellSlots[i] = 0;
              } else if (activeCombatant.spellSlots[i] >
                         activeCombatant.maxSpellSlots[i]) {
                activeCombatant.spellSlots[i] =
                    activeCombatant.maxSpellSlots[i];
              }
            }
          }
        }
        ImGui::EndTable();
      }
    }

  } else {
    ImGui::Text("Player characters manage their own abilities.");
  }
  ImGui::End();
}

std::vector<Ability> getMonsterAbilities(int monsterId, SQLite::Database &db) {
  std::vector<Ability> abilities;
  try {
    // This query performs a LEFT JOIN to gather usage data where it exists.
    SQLite::Statement query(
        db, "SELECT A.Name, A.Description, A.AbilityType, "
            "AU.UsageType, AU.UsesMax, AU.RechargeValue "
            "FROM Abilities AS A "
            "LEFT JOIN Ability_Usage AS AU ON A.AbilityID = AU.AbilityID "
            "WHERE A.MonsterID = ?");
    query.bind(1, monsterId);

    while (query.executeStep()) {
      Ability ability;
      ability.name = query.getColumn(0).getString();
      ability.description = query.getColumn(1).getString();
      ability.type = query.getColumn(2).getString();

      // These columns can be NULL if there's no entry in Ability_Usage
      if (!query.getColumn(3).isNull()) {
        ability.usageType = query.getColumn(3).getString();
        ability.usesMax = query.getColumn(4).getInt();
        ability.rechargeValue = query.getColumn(5).getInt();
      }

      abilities.push_back(ability);
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterAbilities: " << e.what()
              << std::endl;
  }
  return abilities;
}
