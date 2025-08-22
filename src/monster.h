#include <string>
#include <vector>

// A simple structure for a single ability
struct Ability {
  std::string name;
  std::string description;
};

// The main structure to hold all monster data
struct Monster {
  int id;
  std::string name;
  int armorClass;
  int hitPoints;
  std::string hpFormula;

  // We will store abilities as a list of simple structs
  std::vector<Ability> traits;
  std::vector<Ability> actions;
  std::vector<Ability> legendaryActions;

  // We can add spells and other details later
};