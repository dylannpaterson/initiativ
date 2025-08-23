#ifndef MONSTER_H
#define MONSTER_H

#include <iostream>
#include <map> // Required for tracking ability uses
#include <string>
#include <vector>

struct Ability {
  std::string name;
  std::string description;
  std::string type;
};

struct Monster {
  std::string name;
  std::string size;
  std::string type;
  std::string alignment;
  int armorClass;
  int hitPoints;
  std::string hitDice;
  std::vector<std::string> speeds; // Now a vector to hold all speed types
  int strength;
  int dexterity;
  int constitution;
  int intelligence;
  int wisdom;
  int charisma;
  std::string challengeRating;

  // New fields for skills, senses, saving throws, and immunities
  std::vector<std::string> skills;
  std::vector<std::string> senses;
  std::vector<std::string> savingThrows;
  std::vector<std::string> conditionImmunities;
  std::vector<std::string> damageImmunities;
  std::vector<std::string> damageResistances;
  std::vector<std::string> damageVulnerabilities;
  std::vector<Ability> abilities;

  // A simple function to display the monster's core stats
  void display() const {
    std::cout << "Name: " << name << std::endl;
    std::cout << "Size: " << size << std::endl;
    std::cout << "Type: " << type << std::endl;
    std::cout << "Alignment: " << alignment << std::endl;
    std::cout << "Armor Class: " << armorClass << std::endl;
    std::cout << "Hit Points: " << hitPoints << " (" << hitDice << ")"
              << std::endl;
    std::cout << "Speed: ";
    for (const auto &speed : speeds) {
      std::cout << speed << "; ";
    }
    std::cout << std::endl;
    std::cout << "STR: " << strength << " | DEX: " << dexterity
              << " | CON: " << constitution << " | INT: " << intelligence
              << " | WIS: " << wisdom << " | CHA: " << charisma << std::endl;
    std::cout << "Challenge Rating: " << challengeRating << std::endl;
  }
};

struct Combatant {
  Monster base;            // The creature's core statistics
  std::string displayName; // Unique name for this instance, e.g., "Goblin 3"
  int currentHitPoints;
  int initiative;
  bool isPlayer = false; // A flag to identify the party

  // A map to track remaining uses of limited abilities
  // Key: Ability Name, Value: Remaining Uses
  std::map<std::string, int> abilityUses;

  // Constructor to build a Combatant from a Monster
  Combatant(const Monster &monster) : base(monster) {
    displayName = base.name; // Start with the base name
    currentHitPoints = base.hitPoints;
    initiative = 0; // Default initiative

    // Here, you would initialize the abilityUses map based on monster
    // abilities. For now, we will leave it empty as a placeholder for a future
    // step.
  }
  Combatant() = default;
};

#endif // MONSTER_H