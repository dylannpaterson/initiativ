import sqlite3
import requests
import re
import json
from pathlib import Path


# --- A More Robust Path Configuration ---
def find_project_root(marker_file="CMakeLists.txt"):
    """Walks up from the script's location to find the project root."""
    current_path = Path(__file__).resolve().parent
    while current_path != current_path.parent:
        if (current_path / marker_file).exists():
            return current_path
        current_path = current_path.parent
    raise FileNotFoundError(
        f"Could not find project root. Marker file '{marker_file}' not found."
    )


try:
    PROJECT_ROOT = find_project_root()
    DATA_DIR = PROJECT_ROOT / "data"
    DATABASE_PATH = DATA_DIR / "initiativ.sqlite"
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    print(f"Project Root identified at: {PROJECT_ROOT}")
    print(f"Archives will be forged at: {DATABASE_PATH}")
except FileNotFoundError as e:
    print(f"FATAL ERROR: {e}")
    exit(1)

# --- Configuration ---
API_BASE_URL = "https://www.dnd5eapi.co/api/"

# --- Action Type Parsing Constants ---
ACTION_PATTERNS = {
    "Bonus Action": [re.compile(r"as a bonus action", re.IGNORECASE)],
    "Reaction": [re.compile(r"as a reaction", re.IGNORECASE)],
    "Action": [re.compile(r"as an action", re.IGNORECASE)],
}


def classify_ability_action_type(description: str, ability_type: str):
    if ability_type == "Legendary Actions":
        return "Legendary"
    if ability_type == "Lair Actions":
        return "Lair"
    if not description:
        return None
    for action_type, patterns in ACTION_PATTERNS.items():
        if any(pattern.search(description) for pattern in patterns):
            return action_type
    if ability_type == "Actions":
        return "Action"
    return None


def setup_database():
    """Establishes the new, fully normalized database schema."""
    print("Beginning complete overhaul of the Archives...")
    conn = sqlite3.connect(DATABASE_PATH)
    cursor = conn.cursor()

    tables_to_drop = [
        "Monster_Spells",
        "Monster_Skills",
        "Monster_Senses",
        "Monster_DamageImmunities",
        "Monster_DamageResistances",
        "Monster_DamageVulnerabilities",
        "Monster_ConditionImmunities",
        "Monster_SpellSlots",
        "Ability_Usage",
        "Abilities",
        "Monster_Speeds",
        "Spells",
        "Skills",
        "Senses",
        "DamageTypes",
        "Conditions",
        "SavingThrows",
        "Monsters",
    ]
    for table in tables_to_drop:
        cursor.execute(f"DROP TABLE IF EXISTS {table}")

    print("Forging the new, normalized Archives...")

    cursor.execute(
        "CREATE TABLE IF NOT EXISTS DamageTypes (DamageTypeID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE COLLATE NOCASE)"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Conditions (ConditionID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE COLLATE NOCASE)"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Skills (SkillID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE COLLATE NOCASE)"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Senses (SenseID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE COLLATE NOCASE)"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS SavingThrows (SavingThrowID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE COLLATE NOCASE)"
    )

    for sense in [
        "blindsight",
        "darkvision",
        "passive perception",
        "tremorsense",
        "truesight",
    ]:
        cursor.execute("INSERT OR IGNORE INTO Senses (Name) VALUES (?)", (sense,))

    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Monsters (
        MonsterID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE, Size TEXT, Type TEXT, Alignment TEXT, 
        ArmorClass INTEGER, HitPoints_Avg INTEGER, HitPoints_Formula TEXT, 
        Strength INTEGER, Dexterity INTEGER, Constitution INTEGER, Intelligence INTEGER, Wisdom INTEGER, 
        Charisma INTEGER, Languages TEXT, ChallengeRating TEXT,
        SpellSaveDC INTEGER, SpellAttackBonus INTEGER
    )"""
    )

    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Abilities (
        AbilityID INTEGER PRIMARY KEY, MonsterID INTEGER, Name TEXT NOT NULL, Description TEXT,
        AbilityType TEXT, ActionType TEXT, TargetType TEXT, AttackRollType TEXT, SavingThrowType TEXT,
        SavingThrowDC INTEGER, DamageDice TEXT, DamageType TEXT, DamageModifierAbility TEXT,
        FOREIGN KEY (MonsterID) REFERENCES Monsters(MonsterID)
    )"""
    )

    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Ability_Usage (
        AbilityID INTEGER PRIMARY KEY, UsageType TEXT, UsesMax INTEGER, RechargeValue INTEGER,
        FOREIGN KEY (AbilityID) REFERENCES Abilities(AbilityID)
    )"""
    )

    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Spells (
        SpellID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE COLLATE NOCASE, Description TEXT, 
        Level INTEGER, CastingTime TEXT, Range TEXT, Components TEXT, Duration TEXT,
        SavingThrowType TEXT, SavingThrowDC INTEGER, DamageDice TEXT, DamageType TEXT,
        DamageModifierAbility TEXT
    )"""
    )

    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_Skills (MonsterID INTEGER, SkillID INTEGER, Value INTEGER NOT NULL, PRIMARY KEY (MonsterID, SkillID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_SavingThrows (MonsterID INTEGER, SavingThrowID INTEGER, Value INTEGER NOT NULL, PRIMARY KEY (MonsterID, SavingThrowID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_Senses (MonsterID INTEGER, SenseID INTEGER, Value TEXT NOT NULL, PRIMARY KEY (MonsterID, SenseID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_DamageImmunities (MonsterID INTEGER, DamageTypeID INTEGER, PRIMARY KEY (MonsterID, DamageTypeID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_DamageResistances (MonsterID INTEGER, DamageTypeID INTEGER, Note TEXT, PRIMARY KEY (MonsterID, DamageTypeID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_DamageVulnerabilities (MonsterID INTEGER, DamageTypeID INTEGER, PRIMARY KEY (MonsterID, DamageTypeID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_ConditionImmunities (MonsterID INTEGER, ConditionID INTEGER, PRIMARY KEY (MonsterID, ConditionID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_Speeds (MonsterID INTEGER, SpeedType TEXT NOT NULL, Value TEXT NOT NULL, PRIMARY KEY (MonsterID, SpeedType), FOREIGN KEY (MonsterID) REFERENCES Monsters(MonsterID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_Spells (MonsterSpellID INTEGER PRIMARY KEY, MonsterID INTEGER, SpellID INTEGER)"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_SpellSlots (MonsterSlotID INTEGER PRIMARY KEY, MonsterID INTEGER, SpellLevel INTEGER NOT NULL, Slots INTEGER NOT NULL, UNIQUE(MonsterID, SpellLevel))"
    )

    conn.commit()
    conn.close()
    print("New Archives forged successfully.")


def parse_and_store_monster(monster_slug, conn, lookup_data):
    cursor = conn.cursor()
    try:
        response = requests.get(f"{API_BASE_URL}monsters/{monster_slug}")
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        print(
            f"  -> Mission Aborted: Could not retrieve intelligence for {monster_slug}. {e}"
        )
        return

    monster_name = data.get("name")
    cursor.execute("SELECT MonsterID FROM Monsters WHERE Name = ?", (monster_name,))
    if cursor.fetchone():
        return

    print(f"\n--- Interrogating dossier for: {monster_slug.upper()} ---")

    spell_save_dc = 0
    spell_attack_bonus = 0
    all_abilities_raw = (
        data.get("special_abilities", [])
        + data.get("actions", [])
        + data.get("legendary_actions", [])
    )
    for ability in all_abilities_raw:
        if ability.get("name") == "Spellcasting":
            desc = ability.get("desc", "")
            dc_match = re.search(r"spell save DC (\d+)", desc)
            if dc_match:
                spell_save_dc = int(dc_match.group(1))

            attack_match = re.search(r"\+(\d+) to hit with spell attacks", desc)
            if attack_match:
                spell_attack_bonus = int(attack_match.group(1))

            print(
                f"  -> Extracted Spellcasting Intel: DC {spell_save_dc}, Attack Bonus +{spell_attack_bonus}"
            )
            break

    cursor.execute(
        """
        INSERT INTO Monsters (
            Name, Size, Type, Alignment, ArmorClass, HitPoints_Avg, HitPoints_Formula,
            Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma,
            Languages, ChallengeRating, SpellSaveDC, SpellAttackBonus
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            monster_name,
            data.get("size"),
            data.get("type"),
            data.get("alignment"),
            data.get("armor_class", [{}])[0].get("value", 10),
            data.get("hit_points"),
            data.get("hit_dice"),
            data.get("strength"),
            data.get("dexterity"),
            data.get("constitution"),
            data.get("intelligence"),
            data.get("wisdom"),
            data.get("charisma"),
            data.get("languages"),
            data.get("challenge_rating"),
            spell_save_dc,
            spell_attack_bonus,
        ),
    )
    monster_id = cursor.lastrowid
    print(f"  -> Stored '{monster_name}' with MonsterID: {monster_id}")

    # Speeds
    for speed_type, speed_value in data.get("speed", {}).items():
        cursor.execute(
            "INSERT OR IGNORE INTO Monster_Speeds (MonsterID, SpeedType, Value) VALUES (?, ?, ?)",
            (monster_id, speed_type, str(speed_value)),
        )

    # Proficiencies (Skills and Saving Throws)
    for prof in data.get("proficiencies", []):
        name = prof["proficiency"]["name"]
        value = prof["value"]
        if name.startswith("Skill: "):
            skill_name = name.replace("Skill: ", "").lower()
            if skill_name in lookup_data["skills"]:
                cursor.execute(
                    "INSERT OR IGNORE INTO Monster_Skills (MonsterID, SkillID, Value) VALUES (?, ?, ?)",
                    (monster_id, lookup_data["skills"][skill_name], value),
                )
        elif name.startswith("Saving Throw: "):
            st_name = name.replace("Saving Throw: ", "").lower()
            if st_name in lookup_data["savingthrows"]:
                cursor.execute(
                    "INSERT OR IGNORE INTO Monster_SavingThrows (MonsterID, SavingThrowID, Value) VALUES (?, ?, ?)",
                    (monster_id, lookup_data["savingthrows"][st_name], value),
                )

    # Damage Relations
    for rel_type in [
        "damage_vulnerabilities",
        "damage_resistances",
        "damage_immunities",
    ]:
        table_name = f"Monster_{rel_type.replace('_', ' ').title().replace(' ', '')}"
        for dt_name in data.get(rel_type, []):
            dt_name_lower = dt_name.lower()
            if dt_name_lower in lookup_data["damagetypes"]:
                cursor.execute(
                    f"INSERT OR IGNORE INTO {table_name} (MonsterID, DamageTypeID) VALUES (?, ?)",
                    (monster_id, lookup_data["damagetypes"][dt_name_lower]),
                )

    # Condition Immunities
    for cond in data.get("condition_immunities", []):
        cond_name = cond["name"].lower()
        if cond_name in lookup_data["conditions"]:
            cursor.execute(
                "INSERT OR IGNORE INTO Monster_ConditionImmunities (MonsterID, ConditionID) VALUES (?, ?)",
                (monster_id, lookup_data["conditions"][cond_name]),
            )

    # Senses
    for sense_name, sense_value in data.get("senses", {}).items():
        sense_name_lower = sense_name.lower().replace("_", " ")
        if sense_name_lower in lookup_data["senses"]:
            cursor.execute(
                "INSERT OR IGNORE INTO Monster_Senses (MonsterID, SenseID, Value) VALUES (?, ?, ?)",
                (monster_id, lookup_data["senses"][sense_name_lower], str(sense_value)),
            )

    # Abilities and Spells
    ability_sections = {
        "Special Abilities": data.get("special_abilities", []),
        "Actions": data.get("actions", []),
        "Legendary Actions": data.get("legendary_actions", []),
        "Lair Actions": data.get("lair_actions", []),
    }
    for ability_type, abilities in ability_sections.items():
        for ability in abilities:
            action_type = classify_ability_action_type(
                ability.get("desc"), ability_type
            )
            cursor.execute(
                "INSERT INTO Abilities (MonsterID, Name, Description, AbilityType, ActionType) VALUES (?, ?, ?, ?, ?)",
                (
                    monster_id,
                    ability["name"],
                    ability.get("desc"),
                    ability_type,
                    action_type,
                ),
            )
            ability_id = cursor.lastrowid
            if "usage" in ability and ability["usage"]:
                cursor.execute(
                    "INSERT INTO Ability_Usage (AbilityID, UsageType, UsesMax) VALUES (?, ?, ?)",
                    (
                        ability_id,
                        ability["usage"].get("type"),
                        ability["usage"].get("times"),
                    ),
                )
            if ability["name"] == "Spellcasting" and "spellcasting" in ability:
                spellcasting_data = ability["spellcasting"]
                for slot_level, slots in spellcasting_data.get("slots", {}).items():
                    cursor.execute(
                        "INSERT OR IGNORE INTO Monster_SpellSlots (MonsterID, SpellLevel, Slots) VALUES (?, ?, ?)",
                        (monster_id, int(slot_level), slots),
                    )

                cursor.execute("SELECT Name, SpellID FROM Spells")
                spell_lookup = {row[0].lower(): row[1] for row in cursor.fetchall()}

                for spell_info in spellcasting_data.get("spells", []):
                    spell_name = spell_info["name"].lower()
                    if spell_name in spell_lookup:
                        cursor.execute(
                            "INSERT OR IGNORE INTO Monster_Spells (MonsterID, SpellID) VALUES (?, ?)",
                            (monster_id, spell_lookup[spell_name]),
                        )


def populate_spell_grimoire(conn):
    print("\n--- Commencing Operation: Reinforce Spell Grimoire ---")
    cursor = conn.cursor()
    try:
        index_response = requests.get(f"{API_BASE_URL}spells")
        index_response.raise_for_status()
        spell_index = index_response.json().get("results", [])

        for i, spell_ref in enumerate(spell_index):
            spell_slug = spell_ref.get("index")
            spell_response = requests.get(f"{API_BASE_URL}spells/{spell_slug}")
            spell_data = spell_response.json()

            name_lower = spell_data.get("name").lower()
            desc = "\n\n".join(spell_data.get("desc", []))
            level = spell_data.get("level")
            casting_time = spell_data.get("casting_time")

            dc_data = spell_data.get("dc", {})
            saving_throw_type = dc_data.get("dc_type", {}).get("name")

            damage_info = spell_data.get("damage", {})
            damage_type = damage_info.get("damage_type", {}).get("name")
            damage_dice = None
            if damage_info and "damage_at_slot_level" in damage_info:
                damage_dice = damage_info["damage_at_slot_level"].get(str(level))

            cursor.execute(
                """
                INSERT OR IGNORE INTO Spells (Name, Description, Level, CastingTime, SavingThrowType, DamageDice, DamageType) 
                VALUES (?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    name_lower,
                    desc,
                    level,
                    casting_time,
                    saving_throw_type,
                    damage_dice,
                    damage_type,
                ),
            )
        print(f"  -> Transcribed {len(spell_index)} spells.")

    except requests.exceptions.RequestException as e:
        print(f"  -> Mission Aborted: Could not retrieve spell index. {e}")
    print("--- Operation Reinforce Spell Grimoire: Complete ---")


def populate_lookup_table(conn, table_name, api_endpoint, key_name="name"):
    print(f"\n--- Populating '{table_name}' lookup table ---")
    cursor = conn.cursor()
    try:
        response = requests.get(f"{API_BASE_URL}{api_endpoint}")
        response.raise_for_status()
        index = response.json().get("results", [])
        items_to_insert = [(item.get(key_name).lower(),) for item in index]
        cursor.executemany(
            f"INSERT OR IGNORE INTO {table_name} (Name) VALUES (?)", items_to_insert
        )
        print(
            f"  -> Success. '{table_name}' table updated with {len(items_to_insert)} entries."
        )
    except requests.exceptions.RequestException as e:
        print(f"  -> FAILED to populate '{table_name}'. Error: {e}")


if __name__ == "__main__":
    setup_database()
    conn = sqlite3.connect(DATABASE_PATH)

    populate_lookup_table(conn, "DamageTypes", "damage-types")
    populate_lookup_table(conn, "Skills", "skills")
    populate_lookup_table(conn, "Conditions", "conditions")
    populate_lookup_table(conn, "SavingThrows", "ability-scores", key_name="index")

    print("\n--- Loading lookup tables into memory for Scribe's use ---")
    cursor = conn.cursor()
    lookup_data = {}
    lookup_map = {
        "skills": "SkillID",
        "savingthrows": "SavingThrowID",
        "damagetypes": "DamageTypeID",
        "conditions": "ConditionID",
        "senses": "SenseID",
    }
    for table, id_col in lookup_map.items():
        cursor.execute(f"SELECT Name, {id_col} FROM {table.title()}")
        lookup_data[table] = {row[0]: row[1] for row in cursor.fetchall()}
    print(" -> Lookup data successfully loaded.")

    populate_spell_grimoire(conn)

    print("\n--- Commencing Full Reconnaissance of All Monster Dossiers ---")
    try:
        monster_index_response = requests.get(f"{API_BASE_URL}monsters")
        monster_index_response.raise_for_status()
        monster_index = monster_index_response.json().get("results", [])
        total_monsters = len(monster_index)
        print(f"Found {total_monsters} monster dossiers to process.")

        for i, monster_ref in enumerate(monster_index):
            monster_slug = monster_ref.get("index")
            parse_and_store_monster(monster_slug, conn, lookup_data)
            conn.commit()

    except requests.exceptions.RequestException as e:
        print(f"  -> CRITICAL FAILURE: Could not retrieve monster index. {e}")

    conn.close()
    print("\nCampaign Phase 0: Complete. The Archives are now fully stocked.")
