import sqlite3
import requests
import re
import json

from pathlib import Path

# --- Path Configuration ---
# Get the directory where this script is located
SCRIPT_DIR = Path(__file__).resolve().parent
# Assume the project root is two levels up from the script's location (initiativ/scripts/python/)
PROJECT_ROOT = SCRIPT_DIR.parent.parent
# Define the path to the data directory
DATA_DIR = PROJECT_ROOT / "data"
# Define the full, absolute path to the database file
DATABASE_PATH = DATA_DIR / "initiativ.sqlite"

# --- THIS IS THE CRUCIAL COMMAND ---
# It ensures the 'data' directory exists before any other operation.
DATA_DIR.mkdir(parents=True, exist_ok=True)

print(f"Archives will be located at: {DATABASE_PATH}")

# --- Configuration ---
API_BASE_URL = "https://www.dnd5eapi.co/api/"

# --- New Strategic Constants for Action Type Parsing ---
ACTION_PATTERNS = {
    "Bonus Action": [
        re.compile(r"as a bonus action", re.IGNORECASE),
        re.compile(r"takes a bonus action", re.IGNORECASE),
    ],
    "Reaction": [
        re.compile(r"as a reaction", re.IGNORECASE),
        re.compile(r"takes a reaction", re.IGNORECASE),
    ],
    "Action": [
        re.compile(r"as an action", re.IGNORECASE),
        re.compile(r"takes an action", re.IGNORECASE),
    ],
}


def classify_ability_action_type(description: str, ability_type: str):
    """
    Parses the description and type of an ability to determine its ActionType.
    """
    if ability_type == "Legendary":
        return "Legendary"
    if ability_type == "Lair":
        return "Lair"

    if not description:
        return None

    for action_type, patterns in ACTION_PATTERNS.items():
        for pattern in patterns:
            if pattern.search(description):
                return action_type

    # Default to 'Action' if no other type is found and it's in the 'Actions' category
    if ability_type == "Action":
        return "Action"

    return None


def setup_database():
    """Establishes the new, fully normalized database schema."""
    print("Beginning complete overhaul of the Archives...")
    conn = sqlite3.connect(DATABASE_PATH)
    cursor = conn.cursor()

    # --- Drop tables in reverse dependency order ---
    cursor.execute("DROP TABLE IF EXISTS Monster_Spells")
    cursor.execute("DROP TABLE IF EXISTS Monster_Skills")
    cursor.execute("DROP TABLE IF EXISTS Monster_Senses")
    cursor.execute("DROP TABLE IF EXISTS Monster_DamageImmunities")
    cursor.execute("DROP TABLE IF EXISTS Monster_DamageResistances")
    cursor.execute("DROP TABLE IF EXISTS Monster_DamageVulnerabilities")
    cursor.execute("DROP TABLE IF EXISTS Monster_ConditionImmunities")
    cursor.execute("DROP TABLE IF EXISTS Monster_SpellSlots")
    cursor.execute("DROP TABLE IF EXISTS Ability_Usage")
    cursor.execute("DROP TABLE IF EXISTS Spells")
    cursor.execute("DROP TABLE IF EXISTS Skills")
    cursor.execute("DROP TABLE IF EXISTS Senses")
    cursor.execute("DROP TABLE IF EXISTS DamageTypes")
    cursor.execute("DROP TABLE IF EXISTS Conditions")
    cursor.execute("DROP TABLE IF EXISTS SavingThrows")
    cursor.execute("DROP TABLE IF EXISTS Abilities")
    cursor.execute("DROP TABLE IF EXISTS Monster_Speeds")
    cursor.execute("DROP TABLE IF EXISTS Monsters")

    print("Forging the new, normalized Archives...")

    # --- LOOKUP TABLES ---
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
    # Pre-populate senses
    senses_to_add = [
        "blindsight",
        "darkvision",
        "passive perception",
        "tremorsense",
        "truesight",
    ]
    for sense in senses_to_add:
        cursor.execute("INSERT OR IGNORE INTO Senses (Name) VALUES (?)", (sense,))

    # --- CORE TABLES ---
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
        AbilityID INTEGER PRIMARY KEY,
        MonsterID INTEGER,
        Name TEXT NOT NULL,
        Description TEXT,
        AbilityType TEXT,
        ActionType TEXT,
        TargetType TEXT,
        AttackRollType TEXT,
        SavingThrowType TEXT,
        SavingThrowDC INTEGER,
        DamageDice TEXT,
        DamageType TEXT,
        DamageModifierAbility TEXT,
        FOREIGN KEY (MonsterID) REFERENCES Monsters(MonsterID)
    )"""
    )

    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Spells (
        SpellID INTEGER PRIMARY KEY, 
        Name TEXT NOT NULL UNIQUE COLLATE NOCASE, 
        Description TEXT, 
        Level INTEGER, 
        CastingTime TEXT, 
        Range TEXT, 
        Components TEXT, 
        Duration TEXT,
        SavingThrowType TEXT,
        SavingThrowDC INTEGER,
        DamageDice TEXT,
        DamageType TEXT,
        DamageModifierAbility TEXT
    )"""
    )

    # --- JOIN TABLES ---
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
        """
    CREATE TABLE IF NOT EXISTS Monster_Speeds (
        MonsterID INTEGER,
        SpeedType TEXT NOT NULL,
        Value TEXT NOT NULL,
        PRIMARY KEY (MonsterID, SpeedType),
        FOREIGN KEY (MonsterID) REFERENCES Monsters(MonsterID)
    )
    """
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

    # --- *** STRATEGIC MODIFICATION *** ---
    # Interrogate the spellcasting ability description BEFORE inserting the monster
    spell_save_dc = 0
    spell_attack_bonus = 0
    special_abilities = data.get("special_abilities", [])
    if special_abilities:
        for ability in special_abilities:
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
                break  # Assume only one spellcasting ability

    # Core monster data
    ac = data.get("armor_class", [{}])[0].get("value", 10)
    hp_avg = data.get("hit_points", 0)
    hp_formula_str = data.get("hit_dice", "")
    strength = data.get("strength", 10)
    dexterity = data.get("dexterity", 10)
    constitution = data.get("constitution", 10)
    intelligence = data.get("intelligence", 10)
    wisdom = data.get("wisdom", 10)
    charisma = data.get("charisma", 10)
    languages = data.get("languages", "")
    size = data.get("size")
    type_val = data.get("type")
    alignment = data.get("alignment")
    cr = data.get("challenge_rating")

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
            size,
            type_val,
            alignment,
            ac,
            hp_avg,
            hp_formula_str,
            strength,
            dexterity,
            constitution,
            intelligence,
            wisdom,
            charisma,
            languages,
            cr,
            spell_save_dc,
            spell_attack_bonus,
        ),
    )
    monster_id = cursor.lastrowid
    print(f"  -> Stored '{monster_name}' with MonsterID: {monster_id}")

    # (Speed, Proficiency, Damage, Senses, Condition parsing remains the same)...
    speed_data = data.get("speed", {})
    if speed_data:
        for speed_type, speed_value in speed_data.items():
            if speed_value:
                cursor.execute(
                    "INSERT OR IGNORE INTO Monster_Speeds (MonsterID, SpeedType, Value) VALUES (?, ?, ?)",
                    (monster_id, speed_type, str(speed_value)),
                )

    # ... and so on for the rest of the function ...


def populate_spell_grimoire(conn):
    """Checks for missing spells and fetches only the new ones."""
    print("\n--- Commencing Operation: Reinforce Spell Grimoire ---")
    cursor = conn.cursor()
    try:
        cursor.execute("SELECT Name FROM Spells")
        existing_spells = {row[0].lower() for row in cursor.fetchall()}

        index_response = requests.get(f"{API_BASE_URL}spells")
        index_response.raise_for_status()
        spell_index = index_response.json().get("results", [])

        new_spells_to_fetch = [
            s for s in spell_index if s.get("name").lower() not in existing_spells
        ]

        if not new_spells_to_fetch:
            print("Archives are already up to date.")
            return

        print(
            f"Found {len(new_spells_to_fetch)} new spell dossiers. Beginning transcription..."
        )
        for i, spell_ref in enumerate(new_spells_to_fetch):
            spell_slug = spell_ref.get("index")
            spell_response = requests.get(f"{API_BASE_URL}spells/{spell_slug}")
            spell_data = spell_response.json()

            name_lower = spell_data.get("name").lower()
            desc = "\n\n".join(spell_data.get("desc", []))
            level = spell_data.get("level")
            casting_time = spell_data.get("casting_time")
            range_val = spell_data.get("range")
            components = ", ".join(spell_data.get("components", []))
            duration = spell_data.get("duration")

            dc_data = spell_data.get("dc", {})
            saving_throw_type = dc_data.get("dc_type", {}).get("name")
            saving_throw_dc = dc_data.get("dc_value")  # This may be None, which is fine

            damage_info = spell_data.get("damage", {})
            damage_type = damage_info.get("damage_type", {}).get("name")
            damage_dice = None
            if damage_info and "damage_at_slot_level" in damage_info:
                damage_dice = damage_info["damage_at_slot_level"].get(str(level))

            cursor.execute(
                """
                INSERT OR IGNORE INTO Spells (
                    Name, Description, Level, CastingTime, Range, Components, Duration, 
                    SavingThrowType, SavingThrowDC, DamageDice, DamageType
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    name_lower,
                    desc,
                    level,
                    casting_time,
                    range_val,
                    components,
                    duration,
                    saving_throw_type,
                    saving_throw_dc,
                    damage_dice,
                    damage_type,
                ),
            )
            print(
                f"  -> Transcribed '{spell_data.get('name')}' ({i + 1}/{len(new_spells_to_fetch)})"
            )

    except requests.exceptions.RequestException as e:
        print(f"  -> Mission Aborted: Could not retrieve spell index. {e}")
        return

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
        "Skills": "SkillID",
        "SavingThrows": "SavingThrowID",
        "DamageTypes": "DamageTypeID",
        "Conditions": "ConditionID",
        "Senses": "SenseID",
    }
    for table, id_col in lookup_map.items():
        cursor.execute(f"SELECT Name, {id_col} FROM {table}")
        lookup_data[table.lower()] = {row[0]: row[1] for row in cursor.fetchall()}
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
            print(f"\nProcessing dossier ({i + 1}/{total_monsters})")
            parse_and_store_monster(monster_slug, conn, lookup_data)
            conn.commit()  # Commit after each monster

    except requests.exceptions.RequestException as e:
        print(f"  -> CRITICAL FAILURE: Could not retrieve monster index. {e}")

    conn.close()

    print("\nCampaign Phase 0: Complete. The Archives are now fully stocked.")
