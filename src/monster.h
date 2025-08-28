#pragma once
#include <map>
#include <string>
#include <vector>

// An enum for clarity and to prevent trivial errors
enum class ActionType { NONE, ACTION, BONUS_ACTION, REACTION, LEGENDARY, LAIR };

struct Ability {
  std::string name;
  std::string description;
  std::string type;
  std::string usageType;
  int usesMax = 0;
  int rechargeValue = 0;
  ActionType actionType = ActionType::NONE; // The cost of using the ability
};

struct Monster {
  std::string name;
  std::string size;
  std::string type;
  std::string alignment;
  int armorClass;
  int hitPoints;
  std::string hitDice;
  int strength;
  int dexterity;
  int constitution;
  int intelligence;
  int wisdom;
  int charisma;
  std::string challengeRating;
  std::string languages;

  // Using vectors to hold the various details from join tables
  std::vector<std::string> speeds;
  std::vector<std::string> skills;
  std::vector<std::string> savingThrows;
  std::vector<std::string> senses;
  std::vector<std::string> conditionImmunities;
  std::vector<std::string> damageImmunities;
  std::vector<std::string> damageResistances;
  std::vector<std::string> damageVulnerabilities;
  std::vector<Ability> abilities;
  std::vector<int> spellSlots;
};

struct Combatant {
  Monster base; // The creature's core statistics
  std::string displayName;
  int initiative = 0;
  int currentHitPoints = 0;
  int maxHitPoints = 0;
  bool isPlayer = false;

  // Tracking for limited-use abilities
  std::map<std::string, int> abilityUses;

  // Spell slot tracking
  std::vector<int> spellSlots;    // Current slots
  std::vector<int> maxSpellSlots; // Maximum slots

  // --- New Strategic Variables ---
  bool hasUsedAction = false;
  bool hasUsedBonusAction = false;
  // bool hasUsedReaction = false; // For future campaigns

  Combatant() = default;
  explicit Combatant(const Monster &monster)
      : base(monster), displayName(monster.name),
        currentHitPoints(monster.hitPoints), maxHitPoints(monster.hitPoints) {
    // Initialize ability uses from the base monster's abilities
    for (const auto &ability : base.abilities) {
      if (ability.usesMax > 0) {
        abilityUses[ability.name] = ability.usesMax;
      }
    }
    // Initialize spell slots
    spellSlots = monster.spellSlots;
    maxSpellSlots = monster.spellSlots;
  }
};