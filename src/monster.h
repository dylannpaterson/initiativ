#pragma once

#include <map>
#include <memory> // For std::unique_ptr
#include <string>
#include <vector>

// An enum for clarity and to prevent trivial errors
enum class ActionType { NONE, ACTION, BONUS_ACTION, REACTION, LEGENDARY, LAIR };
enum class TriggerCondition {
  ALWAYS,
  ON_HIT,
  ON_MISS,
  ON_SAVE_SUCCESS,
  ON_SAVE_FAIL
};

// --- The Core of the New Engine: The Effect Tree ---
struct Effect {
  // --- Core Mechanics ---
  std::string description;
  std::string attackRollType;
  std::string savingThrowType;
  int savingThrowDC = 0;
  std::string damageDice;
  std::string damageType;
  std::string damageModifierAbility;
  std::string conditionToApply;

  // --- Chaining Logic ---
  TriggerCondition trigger = TriggerCondition::ALWAYS;
  std::vector<std::unique_ptr<Effect>> childEffects;

  // --- Rule of Five for correct resource management of unique_ptr ---
  Effect() = default;

  // 1. Copy Constructor (for deep copying the effect tree)
  Effect(const Effect &other)
      : description(other.description), attackRollType(other.attackRollType),
        savingThrowType(other.savingThrowType),
        savingThrowDC(other.savingThrowDC), damageDice(other.damageDice),
        damageType(other.damageType),
        damageModifierAbility(other.damageModifierAbility),
        conditionToApply(other.conditionToApply), trigger(other.trigger) {
    childEffects.reserve(other.childEffects.size());
    for (const auto &child : other.childEffects) {
      childEffects.push_back(std::make_unique<Effect>(*child));
    }
  }

  // 2. Copy Assignment Operator
  Effect &operator=(const Effect &other) {
    if (this != &other) {
      description = other.description;
      attackRollType = other.attackRollType;
      savingThrowType = other.savingThrowType;
      savingThrowDC = other.savingThrowDC;
      damageDice = other.damageDice;
      damageType = other.damageType;
      damageModifierAbility = other.damageModifierAbility;
      conditionToApply = other.conditionToApply;
      trigger = other.trigger;

      childEffects.clear();
      childEffects.reserve(other.childEffects.size());
      for (const auto &child : other.childEffects) {
        childEffects.push_back(std::make_unique<Effect>(*child));
      }
    }
    return *this;
  }

  // 3. Move Constructor (efficiently transfers ownership)
  Effect(Effect &&other) noexcept = default;
  // 4. Move Assignment Operator
  Effect &operator=(Effect &&other) noexcept = default;
  // 5. Destructor (implicitly handled by unique_ptr)
  ~Effect() = default;
};

struct Ability {
  std::string name;
  std::string description;
  ActionType actionType = ActionType::NONE;
  std::string type;
  std::string usageType;
  int usesMax = 0;
  int rechargeValue = 0;
  std::vector<std::unique_ptr<Effect>> rootEffects;

  // --- Rule of Five for correct copying of abilities ---
  Ability() = default;

  Ability(const Ability &other)
      : name(other.name), description(other.description),
        actionType(other.actionType), type(other.type),
        usageType(other.usageType), usesMax(other.usesMax),
        rechargeValue(other.rechargeValue) {
    rootEffects.reserve(other.rootEffects.size());
    for (const auto &effect : other.rootEffects) {
      rootEffects.push_back(std::make_unique<Effect>(*effect));
    }
  }

  Ability &operator=(const Ability &other) {
    if (this != &other) {
      name = other.name;
      description = other.description;
      actionType = other.actionType;
      type = other.type;
      usageType = other.usageType;
      usesMax = other.usesMax;
      rechargeValue = other.rechargeValue;

      rootEffects.clear();
      rootEffects.reserve(other.rootEffects.size());
      for (const auto &effect : other.rootEffects) {
        rootEffects.push_back(std::make_unique<Effect>(*effect));
      }
    }
    return *this;
  }

  Ability(Ability &&other) noexcept = default;
  Ability &operator=(Ability &&other) noexcept = default;
  ~Ability() = default;
};

struct Spell {
  std::string name;
  std::string description;
  int level;
  ActionType actionType = ActionType::NONE;
  std::vector<std::unique_ptr<Effect>> rootEffects;

  // --- Rule of Five for correct copying of spells ---
  Spell() = default;

  Spell(const Spell &other)
      : name(other.name), description(other.description), level(other.level),
        actionType(other.actionType) {
    rootEffects.reserve(other.rootEffects.size());
    for (const auto &effect : other.rootEffects) {
      rootEffects.push_back(std::make_unique<Effect>(*effect));
    }
  }

  Spell &operator=(const Spell &other) {
    if (this != &other) {
      name = other.name;
      description = other.description;
      level = other.level;
      actionType = other.actionType;

      rootEffects.clear();
      rootEffects.reserve(other.rootEffects.size());
      for (const auto &effect : other.rootEffects) {
        rootEffects.push_back(std::make_unique<Effect>(*effect));
      }
    }
    return *this;
  }

  Spell(Spell &&other) noexcept = default;
  Spell &operator=(Spell &&other) noexcept = default;
  ~Spell() = default;
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

  int spellSaveDC = 0;
  int spellAttackBonus = 0;

  std::vector<std::string> speeds;
  std::vector<std::string> skills;
  std::vector<std::string> savingThrows;
  std::vector<std::string> senses;
  std::vector<std::string> conditionImmunities;
  std::vector<std::string> damageImmunities;
  std::vector<std::string> damageResistances;
  std::vector<std::string> damageVulnerabilities;
  std::vector<Ability> abilities;
  std::vector<Spell> spells;
};

struct Combatant {
  Monster base;
  std::string displayName;
  int initiative = 0;
  int currentHitPoints = 0;
  int maxHitPoints = 0;
  bool isPlayer = false;
  int spellSaveDC = 0;
  int spellAttackBonus = 0;
  std::map<std::string, int> abilityUses;
  std::vector<int> spellSlots;
  std::vector<int> maxSpellSlots;
  bool hasUsedAction = false;
  bool hasUsedBonusAction = false;
  std::vector<std::pair<std::string, int>> activeConditions;

  Combatant() = default;
  explicit Combatant(const Monster &monster)
      : base(monster), displayName(monster.name),
        currentHitPoints(monster.hitPoints), maxHitPoints(monster.hitPoints),
        spellSaveDC(monster.spellSaveDC),
        spellAttackBonus(monster.spellAttackBonus) {
    for (const auto &ability : base.abilities) {
      if (ability.usesMax > 0) {
        abilityUses[ability.name] = ability.usesMax;
      }
    }
  }
};