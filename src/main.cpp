#include "monster.h" // Include our new monster definition
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h> // We will use this with ImGui
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm> // For std::transform
#include <algorithm> // For std::sort
#include <cctype>    // For ::tolower
#include <iostream>
#include <random> // For the casting of lots
// #include <random> // For the casting of lots
#include <sstream>
// #include <stdexcept>
#include <string>
#include <vector>

// ImGui Headers
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include <regex> // For regular expressions

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
static std::mt19937 g_rng(std::random_device{}()); // Random number generator

// --- Targeting State ---
struct TargetingState {
  bool isTargeting = false;
  const Ability *ability = nullptr;
  const Spell *spell = nullptr;
  std::vector<int> selectedTargets;
};
static TargetingState g_targetingState;

// --- Combat Log ---
struct LogEntry {
  enum LogEntryType { DAMAGE, HEALING, EVENT, INFO };
  std::string message;
  LogEntryType type;
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
ActionType stringToActionType(const std::string &str);
void renderTargetingUI();
void resolveAction(TargetingState &targetingState);

// --- Combat Log UI ---
void renderCombatLogUI() {
  ImGui::Begin("Combat Log");
  for (const auto &entry : g_combatLog) {
    ImVec4 color;
    switch (entry.type) {
    case LogEntry::DAMAGE:
      color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
      break;
    case LogEntry::HEALING:
      color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
      break;
    case LogEntry::EVENT:
      color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
      break;
    default:
      color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      break;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", entry.message.c_str());
    ImGui::PopStyleColor();
  }
  // Auto-scroll to the bottom
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::End();
}
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
std::vector<Spell> getMonsterSpells(int monsterId, SQLite::Database &db);

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

std::vector<Spell> getMonsterSpells(int monsterId, SQLite::Database &db) {
  std::vector<Spell> spells;
  try {
    SQLite::Statement query(
        db, "SELECT S.Name, S.Level, S.CastingTime FROM Spells AS S "
            "INNER JOIN Monster_Spells AS MS ON S.SpellID = MS.SpellID "
            "WHERE MS.MonsterID = ? ORDER BY S.Level, S.Name");
    query.bind(1, monsterId);
    while (query.executeStep()) {
      Spell spell;
      spell.name = query.getColumn(0).getString();
      spell.level = query.getColumn(1).getInt();
      spell.actionType = stringToActionType(query.getColumn(2).getString());
      spells.push_back(spell);
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterSpells: " << e.what() << std::endl;
  }
  return spells;
}

// Function to fetch a single monster's details by name
Monster getMonsterByName(SQLite::Database &db, const std::string &monsterName) {
  Monster monster;
  try {
    // We must first get the MonsterID to query the join tables
    SQLite::Statement idQuery(db, "SELECT MonsterID FROM Monsters WHERE Name = ?");
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
    monster.spells = getMonsterSpells(monsterId, db);

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
      renderCombatLogUI(); // Display the combat log
      if (g_targetingState.isTargeting) {
        renderTargetingUI();
      }
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
  io.ConfigFlags |
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags | ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Load a font to improve readability
  io.Fonts->AddFontFromFileTTF("../data/fonts/FiraSans-Regular.ttf", 36.0f);

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

// Helper function to parse dice strings and roll dice
int rollDice(const std::string &diceString) {
  int total = 0;
  std::string s = diceString;
  std::transform(s.begin(), s.end(), s.begin(),
                 ::tolower); // Convert to lowercase

  // Regex to match NdN[+/-M]
  std::regex pattern(R"((\d+)d(\d+)(?:([+-])(\d+))?)");
  std::smatch matches;

  if (std::regex_match(s, matches, pattern)) {
    int numDice = std::stoi(matches[1].str());
    int dieType = std::stoi(matches[2].str());

    for (int i = 0; i < numDice; ++i) { // Use g_rng for random numbers
      total += (std::uniform_int_distribution<>(1, dieType))(g_rng);
    }

    if (matches[3].matched) { // Check if modifier exists
      char sign = matches[3].str()[0];
      int modifier = std::stoi(matches[4].str());
      if (sign == '+') {
        total += modifier;
      } else if (sign == '-') {
        total -= modifier;
      }
    }
  }
  else {
    // Handle cases where it's just a number (e.g., "5")
    try {
      total = std::stoi(diceString);
    } catch (const std::invalid_argument &e) {
      std::cerr << "Error: Invalid dice string format: " << diceString
                << std::endl;
      return 0; // Or throw an exception
    } catch (const std::out_of_range &e) {
      std::cerr << "Error: Dice string out of range: " << diceString
                << std::endl;
      return 0; // Or throw an exception
    }
  }
  return total;
}

// Helper function to get ability score from Combatant based on string name
int getAbilityScore(const Combatant &combatant,
                    const std::string &abilityName) {
  std::string lowerAbilityName = abilityName;
  std::transform(lowerAbilityName.begin(), lowerAbilityName.end(),
                 lowerAbilityName.begin(), ::tolower);

  if (lowerAbilityName == "strength")
    return combatant.base.strength;
  if (lowerAbilityName == "dexterity")
    return combatant.base.dexterity;
  if (lowerAbilityName == "constitution")
    return combatant.base.constitution;
  if (lowerAbilityName == "intelligence")
    return combatant.base.intelligence;
  if (lowerAbilityName == "wisdom")
    return combatant.base.wisdom;
  if (lowerAbilityName == "charisma")
    return combatant.base.charisma;
  return 0; // Default or error
}

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
        ImGui::Text("[%s] %s", ability.type.c_str(), ability.name.c_str());
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

      // --- The Decisive Action ---
      // Manually fetch and assign spell slots to eradicate all ambiguity.
      try {
        SQLite::Statement idQuery(
            *g_db, "SELECT MonsterID FROM Monsters WHERE Name = ?");
        idQuery.bind(1, g_currentMonster.name);
        if (idQuery.executeStep()) {
          int monsterId = idQuery.getColumn(0).getInt();
          newCombatant.spellSlots = getMonsterSpellSlots(monsterId, *g_db);
          newCombatant.maxSpellSlots = newCombatant.spellSlots;
        }
      } catch (const std::exception &e) {
        std::cerr << "Failed to get monster ID for spell slot assignment: "
                  << e.what() << std::endl;
      }
      // --- End Decisive Action ---

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
      {
        std::stringstream ss;
        ss << newCombatant.displayName << " has joined the fray!";
        g_combatLog.push_back({ss.str(), LogEntry::INFO});
      }
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
      {
        std::stringstream ss;
        ss << newPlayer.displayName << " has joined the fray!";
        g_combatLog.push_back({ss.str(), LogEntry::INFO});
      }

      g_newPlayerNameBuffer[0] = '\0';
      g_newPlayerInitiative = 0;
    }
  }

  ImGui::SeparatorText("Combatants");

  if (!g_encounterList.empty()) {
    if (!g_combatHasBegun) {
      if (ImGui::Button("Begin Combat")) {
        g_combatLog.push_back({"Combat has begun!", LogEntry::EVENT});
        for (auto &combatant : g_encounterList) {
          if (!combatant.isPlayer) {
            combatant.initiative =
                (std::uniform_int_distribution<>(1, 20))(g_rng) +
                calculateModifier(combatant.base.dexterity);
          }
        }
        std::sort(g_encounterList.begin(), g_encounterList.end(),
                  [](const Combatant &a, const Combatant &b) {
                    return a.initiative > b.initiative;
                  });
        g_currentTurnIndex = 0;
        if (g_currentTurnIndex >= 0 &&
            g_currentTurnIndex < g_encounterList.size()) {
          g_encounterList[g_currentTurnIndex].hasUsedAction = false;
          g_encounterList[g_currentTurnIndex].hasUsedBonusAction = false;
          std::cout << "BEGIN COMBAT: Resetting actions for "
                    << g_encounterList[g_currentTurnIndex].displayName
                    << std::endl;
        }
        g_combatHasBegun = true; // Raise the banner!
        {
          std::stringstream ss;
          ss << "It is now " << g_encounterList[g_currentTurnIndex].displayName
             << "'s turn.";
          g_combatLog.push_back({ss.str(), LogEntry::EVENT});
        }
      }
    } else {
      if (ImGui::Button("End Combat")) {
        g_currentTurnIndex = -1;
        g_combatHasBegun = false; // Lower the banner
        g_combatLog.push_back({"Combat has ended.", LogEntry::EVENT});
      }
    }

    // "Next/Previous Turn" buttons should only be active during combat
    if (g_combatHasBegun) {
      ImGui::SameLine();
      if (ImGui::Button("Next Turn")) {
        if (g_currentTurnIndex != -1) {
          g_currentTurnIndex =
              (g_currentTurnIndex + 1) % g_encounterList.size();
          // --- Reset Actions for the new turn ---
          g_encounterList[g_currentTurnIndex].hasUsedAction = false;
          g_encounterList[g_currentTurnIndex].hasUsedBonusAction = false;
          std::cout << "NEXT TURN: Resetting actions for "
                    << g_encounterList[g_currentTurnIndex].displayName
                    << std::endl;
          {
            std::stringstream ss;
            ss << "It is now "
               << g_encounterList[g_currentTurnIndex].displayName << "'s turn.";
            g_combatLog.push_back({ss.str(), LogEntry::EVENT});
          }
        }
      }
      ImGui::SameLine();
      // Inside the "Previous Turn" button logic
      if (ImGui::Button("Previous Turn")) {
        if (g_currentTurnIndex != -1) {
          g_currentTurnIndex =
              (g_currentTurnIndex - 1 + g_encounterList.size()) %
              g_encounterList.size();
          // --- Reset Actions for the new turn ---
          g_encounterList[g_currentTurnIndex].hasUsedAction = false;
          g_encounterList[g_currentTurnIndex].hasUsedBonusAction = false;
          {
            std::stringstream ss;
            ss << "It is now "
               << g_encounterList[g_currentTurnIndex].displayName << "'s turn.";
            g_combatLog.push_back({ss.str(), LogEntry::EVENT});
          }
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
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
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
                            std::to_string(g_encounterList[i].initiative) + ")";
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
            {
              std::stringstream ss;
              ss << g_encounterList[i].displayName << " takes 1 damage.";
              g_combatLog.push_back({ss.str(), LogEntry::DAMAGE});
            }
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
            {
              std::stringstream ss;
              ss << g_encounterList[i].displayName << " heals 1 damage.";
              g_combatLog.push_back({ss.str(), LogEntry::HEALING});
            }
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
        g_combatLog.push_back(
            {g_encounterList[combatant_to_remove].displayName +
                 " has been removed from combat.",
             LogEntry::INFO});
        g_encounterList.erase(g_encounterList.begin() + combatant_to_remove);
      }
      ImGui::EndTable();
    }
  }

  ImGui::End();
}
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

  // Display available actions for the turn
  ImGui::Text("Actions Available: ");
  ImGui::SameLine();
  if (!activeCombatant.hasUsedAction)
    ImGui::Text("[Action]");
  else
    ImGui::TextDisabled("[Action]");
  ImGui::SameLine();
  if (!activeCombatant.hasUsedBonusAction)
    ImGui::Text("[Bonus Action]");
  else
    ImGui::TextDisabled("[Bonus Action]");

  if (!activeCombatant.isPlayer) {
    ImGui::SeparatorText("Abilities");
    if (activeCombatant.base.abilities.empty()) {
      ImGui::Text("This creature has no special abilities.");
    } else {
      auto &usesMap = activeCombatant.abilityUses;
      for (const auto &ability : activeCombatant.base.abilities) {
        if (ability.name == "Spellcasting") {
          continue;
        }
        ImGui::PushID(&ability);

        bool is_usable_action =
            (ability.actionType == ActionType::ACTION ||
             ability.actionType == ActionType::BONUS_ACTION);

        ImGui::Text("[%s] %s", ability.type.c_str(), ability.name.c_str());
        ImGui::TextWrapped("%s", ability.description.c_str());

        if (is_usable_action) {
          bool is_limited_by_uses = (ability.usesMax > 0);
          int remaining_uses = is_limited_by_uses ? usesMap[ability.name] : 0;

          bool action_already_used =
              (ability.actionType == ActionType::ACTION &&
               activeCombatant.hasUsedAction) ||
              (ability.actionType == ActionType::BONUS_ACTION &&
               activeCombatant.hasUsedBonusAction);

          if ((is_limited_by_uses && remaining_uses <= 0) ||
              action_already_used) {
            ImGui::BeginDisabled();
          }

          ImGui::SameLine();
          if (ImGui::Button("Use")) {
            g_targetingState.isTargeting = true;
            g_targetingState.ability = &ability;
            g_targetingState.spell = nullptr;
          }

          if ((is_limited_by_uses && remaining_uses <= 0) ||
              action_already_used) {
            ImGui::EndDisabled();
          }
        }

        ImGui::Separator();
        ImGui::PopID();
      }
    }

    // --- Spellcasting Section ---
    if (!activeCombatant.base.spells.empty()) {
      ImGui::SeparatorText("Spells");
      for (const auto &spell : activeCombatant.base.spells) {
        ImGui::PushID(&spell);

        bool has_slots = (spell.level == 0) ||
                         (activeCombatant.spellSlots[spell.level - 1] > 0);
        bool action_available = (spell.actionType == ActionType::ACTION &&
                                 !activeCombatant.hasUsedAction) ||
                                (spell.actionType == ActionType::BONUS_ACTION &&
                                 !activeCombatant.hasUsedBonusAction);

        if (!has_slots || !action_available) {
          ImGui::BeginDisabled();
        }

        ImGui::Text("Lvl %d: %s", spell.level, spell.name.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Cast")) {
          g_targetingState.isTargeting = true;
          g_targetingState.spell = &spell;
          g_targetingState.ability = nullptr;
        }

        if (!has_slots || !action_available) {
          ImGui::EndDisabled();
        }

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
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed,
                                100.0f);
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

void renderTargetingUI() {
  if (!g_targetingState.isTargeting) {
    return;
  }

  ImGui::Begin("Select Target(s)", &g_targetingState.isTargeting);

  const char *actionName = "";
  int maxTargets = 1;
  if (g_targetingState.ability) {
    actionName = g_targetingState.ability->name.c_str();
    // TODO: Add a way to specify max targets for abilities
  } else if (g_targetingState.spell) {
    actionName = g_targetingState.spell->name.c_str();
    // TODO: Add a way to specify max targets for spells
  }

  ImGui::Text("Choose target(s) for %s", actionName);
  ImGui::Separator();

  for (int i = 0; i < g_encounterList.size(); ++i) {
    bool is_selected = false;
    for (int selected_idx : g_targetingState.selectedTargets) {
      if (i == selected_idx) {
        is_selected = true;
        break;
      }
    }

    if (ImGui::Selectable(g_encounterList[i].displayName.c_str(),
                          is_selected)) {
      if (is_selected) {
        // Remove from selection
        g_targetingState.selectedTargets.erase(
            std::remove(g_targetingState.selectedTargets.begin(),
                        g_targetingState.selectedTargets.end(), i),
            g_targetingState.selectedTargets.end());
      } else {
        // Add to selection
        if (g_targetingState.selectedTargets.size() < maxTargets) {
          g_targetingState.selectedTargets.push_back(i);
        }
      }
    }
  }

  ImGui::Separator();

  if (ImGui::Button("Confirm")) {
    resolveAction(g_targetingState);
    g_targetingState.isTargeting = false;
    g_targetingState.selectedTargets.clear();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    g_targetingState.isTargeting = false;
    g_targetingState.selectedTargets.clear();
  }

  ImGui::End();
}

void resolveAction(TargetingState &targetingState) {
  if (!targetingState.isTargeting) {
    return;
  }

  Combatant &activeCombatant = g_encounterList[g_currentTurnIndex];

  if (targetingState.ability) {
    const Ability &ability = *targetingState.ability;
    auto &usesMap = activeCombatant.abilityUses;

    if (ability.usesMax > 0) {
      usesMap[ability.name]--;
    }

    if (ability.actionType == ActionType::ACTION) {
      activeCombatant.hasUsedAction = true;
    } else if (ability.actionType == ActionType::BONUS_ACTION) {
      activeCombatant.hasUsedBonusAction = true;
    }

    std::stringstream log_ss;
    log_ss << activeCombatant.displayName << " uses " << ability.name << ". ";
    g_combatLog.push_back({log_ss.str(), LogEntry::INFO});
    log_ss.str("");

    for (int target_idx : targetingState.selectedTargets) {
      Combatant &target = g_encounterList[target_idx];
      log_ss << target.displayName << ": ";

      bool hit = true;
      if (!ability.attackRollType.empty()) { // It's an attack roll ability
        int attack_roll = rollDice("1d20");
        int attacker_ability_score = getAbilityScore(
            activeCombatant,
            ability.damageModifierAbility); // Assuming ability score is
                                                    // used for attack roll
        int attack_modifier = calculateModifier(attacker_ability_score);
        int total_attack = attack_roll + attack_modifier;

        log_ss << "Attack Roll (" << attack_roll << " + " << attack_modifier
               << ") vs AC " << target.base.armorClass << ". ";
        if (total_attack >= target.base.armorClass) {
          log_ss << "HIT! ";
          hit = true;
        } else {
          log_ss << "MISS! ";
          hit = false;
        }
      } else if (!ability.savingThrowType.empty()) { // It's a saving throw ability
        int save_roll = rollDice("1d20");
        // For simplicity, assuming target's own saving throw modifier
        // A proper implementation would get the target's specific
        // saving throw bonus
        int target_ability_score =
            getAbilityScore(target, ability.savingThrowType);
        int save_modifier = calculateModifier(target_ability_score);
        int total_save = save_roll + save_modifier;

        log_ss << "Save Roll (" << save_roll << " + " << save_modifier
               << ") vs DC " << ability.savingThrowDC << ". ";
        if (total_save >= ability.savingThrowDC) {
          log_ss << "SAVE! ";
          hit = false; // Save means no full effect
        } else {
          log_ss << "FAIL! ";
          hit = true; // Fail means full effect
        }
      }

      if (hit && !ability.damageDice.empty()) { // If it hits and deals damage
        int damage_roll = rollDice(ability.damageDice);
        int damage_modifier = 0;
        if (!ability.damageModifierAbility.empty()) {
          int attacker_ability_score =
              getAbilityScore(activeCombatant, ability.damageModifierAbility);
          damage_modifier = calculateModifier(attacker_ability_score);
        }
        int total_damage = damage_roll + damage_modifier;

        target.currentHitPoints -= total_damage;
        log_ss << "Deals " << total_damage << " " << ability.damageType
               << " damage. ";
        g_combatLog.push_back({log_ss.str(), LogEntry::DAMAGE});
      } else if (!hit && !ability.damageDice.empty() &&
                 !ability.savingThrowType.empty()) { // Half damage on save
        // Assuming half damage on successful save for now
        int damage_roll = rollDice(ability.damageDice);
        int damage_modifier = 0;
        if (!ability.damageModifierAbility.empty()) {
          int attacker_ability_score =
              getAbilityScore(activeCombatant, ability.damageModifierAbility);
          damage_modifier = calculateModifier(attacker_ability_score);
        }
        int total_damage =
            (damage_roll + damage_modifier) / 2; // Half damage
        target.currentHitPoints -= total_damage;
        log_ss << "Deals " << total_damage << " " << ability.damageType
               << " damage (half on save). ";
        g_combatLog.push_back({log_ss.str(), LogEntry::DAMAGE});
      } else {
        g_combatLog.push_back({log_ss.str(), LogEntry::INFO});
      }
    }
  } else if (targetingState.spell) {
    const Spell &spell = *targetingState.spell;

    if (spell.level > 0) {
      activeCombatant.spellSlots[spell.level - 1]--;
    }

    if (spell.actionType == ActionType::ACTION) {
      activeCombatant.hasUsedAction = true;
    } else if (spell.actionType == ActionType::BONUS_ACTION) {
      activeCombatant.hasUsedBonusAction = true;
    }

    std::stringstream log_ss;
    log_ss << activeCombatant.displayName << " casts " << spell.name << ". ";
    g_combatLog.push_back({log_ss.str(), LogEntry::INFO});
    log_ss.str("");

    for (int target_idx : targetingState.selectedTargets) {
      Combatant &target = g_encounterList[target_idx];
      log_ss << target.displayName << ": ";

      bool hit = true;
      if (!spell.attackRollType.empty()) { // It's an attack roll spell
        int attack_roll = rollDice("1d20");
        int caster_ability_score = getAbilityScore(
            activeCombatant,
            spell.damageModifierAbility); // Assuming spellcasting ability
                                              // is used for attack roll
        int attack_modifier = calculateModifier(caster_ability_score);
        int total_attack = attack_roll + attack_modifier;

        log_ss << "Attack Roll (" << attack_roll << " + " << attack_modifier
               << ") vs AC " << target.base.armorClass << ". ";
        if (total_attack >= target.base.armorClass) {
          log_ss << "HIT! ";
          hit = true;
        } else {
          log_ss << "MISS! ";
          hit = false;
        }
      } else if (!spell.savingThrowType.empty()) { // It's a saving throw spell
        int save_roll = rollDice("1d20");
        // For simplicity, assuming target's own saving throw modifier
        // A proper implementation would get the target's specific saving
        // throw bonus
        int target_ability_score =
            getAbilityScore(target, spell.savingThrowType);
        int save_modifier = calculateModifier(target_ability_score);
        int total_save = save_roll + save_modifier;

        log_ss << "Save Roll (" << save_roll << " + " << save_modifier
               << ") vs DC " << spell.savingThrowDC << ". ";
        if (total_save >= spell.savingThrowDC) {
          log_ss << "SAVE! ";
          hit = false; // Save means no full effect
        } else {
          log_ss << "FAIL! ";
          hit = true; // Fail means full effect
        }
      }

      if (hit && !spell.damageDice.empty()) { // If it hits and deals damage
        int damage_roll = rollDice(spell.damageDice);
        int damage_modifier = 0;
        if (!spell.damageModifierAbility.empty()) {
          int caster_ability_score =
              getAbilityScore(activeCombatant, spell.damageModifierAbility);
          damage_modifier = calculateModifier(caster_ability_score);
        }
        int total_damage = damage_roll + damage_modifier;

        target.currentHitPoints -= total_damage;
        log_ss << "Deals " << total_damage << " " << spell.damageType
               << " damage. ";
        g_combatLog.push_back({log_ss.str(), LogEntry::DAMAGE});
      } else if (!hit && !spell.damageDice.empty() &&
                 !spell.savingThrowType.empty()) { // Half damage on save
        // Assuming half damage on successful save for now
        int damage_roll = rollDice(spell.damageDice);
        int damage_modifier = 0;
        if (!spell.damageModifierAbility.empty()) {
          int caster_ability_score =
              getAbilityScore(activeCombatant, spell.damageModifierAbility);
          damage_modifier = calculateModifier(caster_ability_score);
        }
        int total_damage = (damage_roll + damage_modifier) / 2; // Half damage
        target.currentHitPoints -= total_damage;
        log_ss << "Deals " << total_damage << " " << spell.damageType
               << " damage (half on save). ";
        g_combatLog.push_back({log_ss.str(), LogEntry::DAMAGE});
      } else {
        g_combatLog.push_back({log_ss.str(), LogEntry::INFO});
      }
    }
  }
}

// A helper to convert strings from the database to our new enum
ActionType stringToActionType(const std::string &str) {
  std::string lower_str = str;
  std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  std::cerr << "DEBUG: stringToActionType received: '" << str
            << "' (lowercased: '" << lower_str << "')" << std::endl;

  if (lower_str.find("bonus action") != std::string::npos) {
    std::cerr << "DEBUG: Matched bonus action"
              << std::endl;
    return ActionType::BONUS_ACTION;
  }
  if (lower_str.find("action") != std::string::npos) {
    std::cerr << "DEBUG: Matched action"
              << std::endl;
    return ActionType::ACTION;
  }
  if (lower_str.find("reaction") != std::string::npos) {
    std::cerr << "DEBUG: Matched reaction"
              << std::endl;
    return ActionType::REACTION;
  }
  if (lower_str.find("legendary") != std::string::npos) {
    std::cerr << "DEBUG: Matched legendary"
              << std::endl;
    return ActionType::LEGENDARY;
  }
  if (lower_str.find("lair") != std::string::npos) {
    std::cerr << "DEBUG: Matched lair"
              << std::endl;
    return ActionType::LAIR;
  }
  std::cerr << "DEBUG: No match, returning NONE" << std::endl;
  return ActionType::NONE;
}

std::vector<Ability> getMonsterAbilities(int monsterId, SQLite::Database &db) {
  std::vector<Ability> abilities;
  try {
    SQLite::Statement query(
        db,
        "SELECT A.Name, A.Description, A.AbilityType, "
        "AU.UsageType, AU.UsesMax, AU.RechargeValue, A.ActionType, "
        "A.TargetType, A.AttackRollType, A.SavingThrowType, A.SavingThrowDC, "
        "A.DamageDice, A.DamageType, A.DamageModifierAbility "
        "FROM Abilities AS A "
        "LEFT JOIN Ability_Usage AS AU ON A.AbilityID = AU.AbilityID "
        "WHERE A.MonsterID = ?");
    query.bind(1, monsterId);

    while (query.executeStep()) {
      Ability ability;
      ability.name = query.getColumn(0).getString();
      ability.description = query.getColumn(1).getString();
      ability.type = query.getColumn(2).getString();

      if (!query.getColumn(3).isNull()) {
        ability.usageType = query.getColumn(3).getString();
        ability.usesMax = query.getColumn(4).getInt();
        ability.rechargeValue = query.getColumn(5).getInt();
      }

      // Read and set the ActionType
      if (!query.getColumn(6).isNull()) {
        ability.actionType = stringToActionType(query.getColumn(6).getString());
      } else {
        ability.actionType = ActionType::NONE;
      }

      // Read and set the new fields
      ability.targetType = query.getColumn(7).getString();
      ability.attackRollType = query.getColumn(8).getString();
      ability.savingThrowType = query.getColumn(9).getString();
      ability.savingThrowDC = query.getColumn(10).getInt();
      ability.damageDice = query.getColumn(11).getString();
      ability.damageType = query.getColumn(12).getString();
      ability.damageModifierAbility = query.getColumn(13).getString();

      abilities.push_back(ability);
    }
  } catch (const std::exception &e) {
    std::cerr << "SQLite error in getMonsterAbilities: " << e.what()
              << std::endl;
  }
  return abilities;
}
