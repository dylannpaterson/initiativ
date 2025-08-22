import sqlite3
import requests
import re
import json

from pathlib import Path

# --- Path Configuration ---
# Get the directory where this script is located
SCRIPT_DIR = Path(__file__).resolve().parent
# Assume the project root is two levels up from the script's location (initiativ/scripts/python/)
PROJECT_ROOT = SCRIPT_DIR.parent
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


def setup_database():
    """Establishes the new, fully normalized database schema."""
    print("Beginning complete overhaul of the Archives...")
    conn = sqlite3.connect(DATABASE_PATH)
    cursor = conn.cursor()

    # --- For development, we drop tables in reverse dependency order ---
    cursor.execute("DROP TABLE IF EXISTS Monster_Spells")
    cursor.execute("DROP TABLE IF EXISTS Monster_Skills")
    cursor.execute("DROP TABLE IF EXISTS Monster_Senses")
    cursor.execute("DROP TABLE IF EXISTS Monster_DamageImmunities")
    cursor.execute("DROP TABLE IF EXISTS Monster_DamageResistances")
    cursor.execute("DROP TABLE IF EXISTS Monster_DamageVulnerabilities")
    cursor.execute("DROP TABLE IF EXISTS Monster_ConditionImmunities")
    cursor.execute("DROP TABLE IF EXISTS Monster_SpellSlots")
    # Lookup Tables
    cursor.execute("DROP TABLE IF EXISTS Spells")
    cursor.execute("DROP TABLE IF EXISTS Skills")
    cursor.execute("DROP TABLE IF EXISTS Senses")
    cursor.execute("DROP TABLE IF EXISTS DamageTypes")
    cursor.execute("DROP TABLE IF EXISTS Conditions")
    # Core Tables
    cursor.execute("DROP TABLE IF EXISTS Abilities")
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

    # --- CORE TABLES ---
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Monsters (
        MonsterID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE, ArmorClass INTEGER, 
        HitPoints_Avg INTEGER, HitPoints_Formula TEXT, Speed TEXT, Strength INTEGER, 
        Dexterity INTEGER, Constitution INTEGER, Intelligence INTEGER, Wisdom INTEGER, 
        Charisma INTEGER, Languages TEXT
    )"""
    )
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Abilities (
        AbilityID INTEGER PRIMARY KEY, MonsterID INTEGER, Name TEXT NOT NULL, 
        Description TEXT, AbilityType TEXT, FOREIGN KEY (MonsterID) REFERENCES Monsters(MonsterID)
    )"""
    )

    # --- JOIN TABLES ---
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_Skills (MonsterID INTEGER, SkillID INTEGER, Value INTEGER NOT NULL, PRIMARY KEY (MonsterID, SkillID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_SavingThrows (MonsterID INTEGER, SavingThrowID INTEGER, Value INTEGER NOT NULL, PRIMARY KEY (MonsterID, SavingThrowID))"
    )  # The new join table
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

    # --- SPELL TABLES ---
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Spells (SpellID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE COLLATE NOCASE, Description TEXT, Level INTEGER, CastingTime TEXT, Range TEXT, Components TEXT, Duration TEXT)"
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


def populate_spell_grimoire(conn):
    """Checks for missing spells and fetches only the new ones."""
    print("\n--- Commencing Operation: Reinforce Spell Grimoire ---")
    cursor = conn.cursor()

    try:
        cursor.execute("SELECT Name FROM Spells")
        existing_spells = {row[0].lower() for row in cursor.fetchall()}
        print(f"Archives currently hold {len(existing_spells)} spell dossiers.")

        index_response = requests.get(f"{API_BASE_URL}spells")
        index_response.raise_for_status()
        spell_index = index_response.json().get("results", [])

        new_spells_to_fetch = []
        for spell_ref in spell_index:
            if spell_ref.get("name").lower() not in existing_spells:
                new_spells_to_fetch.append(spell_ref)

        if not new_spells_to_fetch:
            print("Archives are already up to date. No new spells to transcribe.")
            return

        print(
            f"Found {len(new_spells_to_fetch)} new spell dossiers. Beginning transcription..."
        )
        for i, spell_ref in enumerate(new_spells_to_fetch):
            spell_slug = spell_ref.get("index")
            spell_response = requests.get(f"{API_BASE_URL}spells/{spell_slug}")
            spell_data = spell_response.json()

            name = spell_data.get("name")
            name_lower = name.lower()
            desc = "\n\n".join(spell_data.get("desc", []))
            level = spell_data.get("level")
            casting_time = spell_data.get("casting_time")
            range_val = spell_data.get("range")
            components = ", ".join(spell_data.get("components", []))
            duration = spell_data.get("duration")

            cursor.execute(
                "INSERT OR IGNORE INTO Spells (Name) VALUES (?)", (name_lower,)
            )
            cursor.execute(
                """
                UPDATE Spells 
                SET Description = ?, Level = ?, CastingTime = ?, Range = ?, Components = ?, Duration = ?
                WHERE Name = ?
            """,
                (
                    desc,
                    level,
                    casting_time,
                    range_val,
                    components,
                    duration,
                    name_lower,
                ),
            )
            print(f"  -> Transcribed '{name}' ({i + 1}/{len(new_spells_to_fetch)})")

    except requests.exceptions.RequestException as e:
        print(f"  -> Mission Aborted: Could not retrieve spell index. {e}")
        return

    print("--- Operation Reinforce Spell Grimoire: Complete ---")


def parse_and_store_monster(monster_slug, conn, lookup_data):
    """
    Fetches, parses, and stores a single monster's data, including core attributes.
    This function will be expanded to handle all normalized data.
    """
    cursor = conn.cursor()

    # Fetch the data from the API
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

    # Check if monster already exists in the Archives
    cursor.execute("SELECT MonsterID FROM Monsters WHERE Name = ?", (monster_name,))
    result = cursor.fetchone()
    if result:
        # This check will be useful once the full script is running
        # print(f"\n--- Dossier for '{monster_name}' already in Archives. Skipping. ---")
        return

    print(f"\n--- Interrogating dossier for: {monster_slug.upper()} ---")

    # --- 1. Parse Core Information & Attributes ---
    ac = data.get("armor_class", [{}])[0].get("value", 10)
    hp_avg = data.get("hit_points")
    hp_formula = data.get("hit_points_dice")
    speed = json.dumps(data.get("speed", {}))

    # Parse the six core attributes
    strength = data.get("strength", 10)
    dexterity = data.get("dexterity", 10)
    constitution = data.get("constitution", 10)
    intelligence = data.get("intelligence", 10)
    wisdom = data.get("wisdom", 10)
    charisma = data.get("charisma", 10)

    # Languages are a simple text field for now
    languages = data.get("languages", "")

    # --- 2. Insert Core Data into Monsters Table ---
    cursor.execute(
        """
        INSERT INTO Monsters (
            Name, ArmorClass, HitPoints_Avg, HitPoints_Formula, Speed,
            Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma,
            Languages
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            monster_name,
            ac,
            hp_avg,
            hp_formula,
            speed,
            strength,
            dexterity,
            constitution,
            intelligence,
            wisdom,
            charisma,
            languages,
        ),
    )
    monster_id = cursor.lastrowid
    print(f"  -> Stored '{monster_name}' with MonsterID: {monster_id}")

    # --- 3. Parse and Store Proficiencies (SKILLS) ---
    proficiencies = data.get("proficiencies", [])
    skills_map = lookup_data.get("skills", {})
    saving_throws_map = lookup_data.get("savingthrows", {})

    for prof in proficiencies:
        prof_name = prof.get("proficiency", {}).get("name", "")
        prof_value = prof.get("value")

        if prof_name.startswith("Skill: "):
            skill_name = prof_name.replace("Skill: ", "").lower()
            if skill_name in skills_map:
                skill_id = skills_map[skill_name]
                cursor.execute(
                    "INSERT INTO Monster_Skills (MonsterID, SkillID, Value) VALUES (?, ?, ?)",
                    (monster_id, skill_id, prof_value),
                )

        elif prof_name.startswith("Saving Throw: "):
            st_name = prof_name.replace("Saving Throw: ", "").lower()
            if st_name in saving_throws_map:
                st_id = saving_throws_map[st_name]

                # --- NEW DIAGNOSTIC PRINT STATEMENT ---
                print(
                    f"  -> DEBUG: Attempting to INSERT into Monster_SavingThrows with MonsterID={monster_id}, SavingThrowID={st_id} ({st_name}), Value={prof_value}"
                )

                cursor.execute(
                    "INSERT INTO Monster_SavingThrows (MonsterID, SavingThrowID, Value) VALUES (?, ?, ?)",
                    (monster_id, st_id, prof_value),
                )

    damage_types_map = lookup_data.get("damagetypes", {})

    process_damage_list(
        monster_id,
        data.get("damage_vulnerabilities", []),
        "Monster_DamageVulnerabilities",
        damage_types_map,
    )
    process_damage_list(
        monster_id,
        data.get("damage_resistances", []),
        "Monster_DamageResistances",
        damage_types_map,
    )
    process_damage_list(
        monster_id,
        data.get("damage_immunities", []),
        "Monster_DamageImmunities",
        damage_types_map,
    )

    ability_sections = {
        "Trait": data.get("special_abilities", []),
        "Action": data.get("actions", []),
        "Legendary": data.get("legendary_actions", []),
    }

    for ability_type, abilities in ability_sections.items():
        for ability_data in abilities:
            ability_name = ability_data.get("name")
            ability_desc = ability_data.get("desc")

            usage_match_recharge = re.search(r"\(Recharge (\d+)-(\d+)\)", ability_name)
            usage_match_per_day = re.search(r"\((\d+)/Day\)", ability_name)

            cursor.execute(
                "INSERT INTO Abilities (MonsterID, Name, Description, AbilityType) VALUES (?, ?, ?, ?)",
                (monster_id, ability_name, ability_desc, ability_type),
            )
            ability_id = cursor.lastrowid

            if usage_match_recharge:
                recharge_value = int(usage_match_recharge.group(1))
                cursor.execute(
                    "INSERT INTO Ability_Usage (AbilityID, UsageType, RechargeValue) VALUES (?, ?, ?)",
                    (ability_id, "Recharge", recharge_value),
                )

            if usage_match_per_day:
                uses_max = int(usage_match_per_day.group(1))
                cursor.execute(
                    "INSERT INTO Ability_Usage (AbilityID, UsageType, UsesMax) VALUES (?, ?, ?)",
                    (ability_id, "Per Day", uses_max),
                )

            if ability_name == "Spellcasting":
                spells_found = 0
                lines = ability_desc.split("\n")
                for line in lines:
                    if ":" in line:
                        parts = line.split(":", 1)
                        if len(parts) == 2:
                            spell_list_str = parts[1]
                            spell_names = [
                                spell.strip().replace(".", "")
                                for spell in spell_list_str.split(",")
                            ]
                            for spell_name in spell_names:
                                if not spell_name:
                                    continue
                                spells_found += 1
                                spell_name_lower = spell_name.lower()
                                cursor.execute(
                                    "SELECT SpellID FROM Spells WHERE Name = ?",
                                    (spell_name_lower,),
                                )
                                spell_id_result = cursor.fetchone()
                                if spell_id_result:
                                    spell_id = spell_id_result[0]
                                    cursor.execute(
                                        "INSERT INTO Monster_Spells (MonsterID, SpellID) VALUES (?, ?)",
                                        (monster_id, spell_id),
                                    )
                if spells_found > 0:
                    print(f"  -> Successfully linked {spells_found} spells.")


def populate_lookup_table(conn, table_name, api_endpoint, key_name="name"):
    """Populates a lookup table, now correctly accepting the key_name argument."""
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


# A helper function to parse these complex strings
def process_damage_list(monster_id, damage_list, table_name, damage_types_map):
    if not damage_list:
        return

    print(f"  -> Analyzing {table_name.replace('_', ' ').lower()}...")
    for item_str in damage_list:
        note = ""
        found_type = None

        # Find the core damage type within the string
        for dmg_type, dmg_id in damage_types_map.items():
            if dmg_type in item_str.lower():
                found_type = dmg_type
                # Any remaining text is a note
                note_parts = item_str.lower().split(dmg_type)
                note = "".join(note_parts).strip()
                break  # Stop after finding the first match

        if found_type:
            type_id = damage_types_map[found_type]

            # Resistances table has a 'Note' column, others do not
            if "Resistances" in table_name and note:
                cursor.execute(
                    f"INSERT INTO {table_name} (MonsterID, DamageTypeID, Note) VALUES (?, ?, ?)",
                    (monster_id, type_id, note),
                )
            else:
                cursor.execute(
                    f"INSERT INTO {table_name} (MonsterID, DamageTypeID) VALUES (?, ?)",
                    (monster_id, type_id),
                )
        else:
            # If no known damage type, store the whole string as a note for bludgeoning (as a fallback)
            # This handles "bludgeoning, piercing, and slashing from nonmagical attacks"
            if "bludgeoning" in item_str.lower():
                bludgeoning_id = damage_types_map.get("bludgeoning")
                if bludgeoning_id:
                    cursor.execute(
                        f"INSERT INTO {table_name} (MonsterID, DamageTypeID, Note) VALUES (?, ?, ?)",
                        (monster_id, bludgeoning_id, item_str),
                    )


if __name__ == "__main__":
    setup_database()
    conn = sqlite3.connect(DATABASE_PATH)

    # Populate all lookup tables (removed duplicate call)
    populate_lookup_table(conn, "DamageTypes", "damage-types")
    populate_lookup_table(conn, "Skills", "skills")
    populate_lookup_table(conn, "Conditions", "conditions")
    populate_lookup_table(conn, "SavingThrows", "ability-scores", key_name="index")

    # Pre-load lookup data into memory
    print("\n--- Loading lookup tables into memory for Scribe's use ---")
    cursor = conn.cursor()
    lookup_data = {}
    lookup_map = {
        "Skills": "SkillID",
        "SavingThrows": "SavingThrowID",
        "DamageTypes": "DamageTypeID",
        "Conditions": "ConditionID",
    }
    for table, id_col in lookup_map.items():
        cursor.execute(f"SELECT Name, {id_col} FROM {table}")
        lookup_data[table.lower()] = {row[0]: row[1] for row in cursor.fetchall()}
    print(" -> Lookup data successfully loaded.")

    populate_spell_grimoire(conn)

    # --- Launch full monster reconnaissance ---
    print("\n--- Commencing Full Reconnaissance of All Monster Dossiers ---")
    try:
        # Get the index of all available monsters.
        monster_index_response = requests.get(f"{API_BASE_URL}monsters")
        monster_index_response.raise_for_status()
        monster_index = monster_index_response.json().get("results", [])

        total_monsters = len(monster_index)
        print(f"Found {total_monsters} monster dossiers to process.")

        # Loop through the entire index and dispatch the Scribe for each.
        for i, monster_ref in enumerate(monster_index):
            monster_slug = monster_ref.get("index")
            print(f"\nProcessing dossier ({i + 1}/{total_monsters})")
            parse_and_store_monster(monster_slug, conn, lookup_data)

    except requests.exceptions.RequestException as e:
        print(f"  -> CRITICAL FAILURE: Could not retrieve monster index. {e}")

    # Step 5: Commit all gathered intelligence and close the connection.
    conn.commit()
    conn.close()

    print("\nCampaign Phase 0: Complete. The Archives are now fully stocked.")
