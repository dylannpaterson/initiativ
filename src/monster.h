#ifndef MONSTER_H
#define MONSTER_H

#include <iostream>
#include <string>

struct Monster {
  std::string name;
  std::string size;
  std::string type;
  std::string alignment;
  int armorClass;
  int hitPoints;
  std::string speed;
  int strength;
  int dexterity;
  int constitution;
  int intelligence;
  int wisdom;
  int charisma;
  std::string challengeRating;
  // We will add more fields like skills, senses, actions, etc. later.
  // For now, this is a solid foundation.

  // A simple function to display the monster's core stats
  void display() const {
    std::cout << "Name: " << name << std::endl;
    std::cout << "Size: " << size << std::endl;
    std::cout << "Type: " << type << std::endl;
    std::cout << "Alignment: " << alignment << std::endl;
    std::cout << "Armor Class: " << armorClass << std::endl;
    std::cout << "Hit Points: " << hitPoints << std::endl;
    std::cout << "Speed: " << speed << std::endl;
    std::cout << "STR: " << strength << " | DEX: " << dexterity
              << " | CON: " << constitution << " | INT: " << intelligence
              << " | WIS: " << wisdom << " | CHA: " << charisma << std::endl;
    std::cout << "Challenge Rating: " << challengeRating << std::endl;
  }
};

#endif // MONSTER_H