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
    cursor.execute("DROP TABLE IF EXISTS Ability_Usage")
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

    # --- Manually add the known senses since there is no API endpoint for them ---
    cursor.execute("INSERT OR IGNORE INTO Senses (Name) VALUES (?)", ("blindsight",))
    cursor.execute("INSERT OR IGNORE INTO Senses (Name) VALUES (?)", ("darkvision",))
    cursor.execute(
        "INSERT OR IGNORE INTO Senses (Name) VALUES (?)", ("passive perception",)
    )
    cursor.execute("INSERT OR IGNORE INTO Senses (Name) VALUES (?)", ("tremorsense",))
    cursor.execute("INSERT OR IGNORE INTO Senses (Name) VALUES (?)", ("truesight",))

    # --- CORE TABLES ---
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Monsters (
        MonsterID INTEGER PRIMARY KEY, Name TEXT NOT NULL UNIQUE, ArmorClass INTEGER, 
        HitPoints_Avg INTEGER, HitPoints_Formula TEXT, HitPoints_NumDice INTEGER, 
        HitPoints_DieType INTEGER, HitPoints_Modifier INTEGER, Speed TEXT, Strength INTEGER, 
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
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Ability_Usage (
        AbilityUsageID INTEGER PRIMARY KEY,
        AbilityID INTEGER,
        UsageType TEXT NOT NULL,
        RechargeValue INTEGER,
        UsesMax INTEGER,
        FOREIGN KEY (AbilityID) REFERENCES Abilities(AbilityID)
    )"""
    )

    # --- JOIN TABLES ---
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_Skills (MonsterID INTEGER, SkillID INTEGER, Value INTEGER NOT NULL, PRIMARY KEY (MonsterID, SkillID))"
    )
    cursor.execute(
        "CREATE TABLE IF NOT EXISTS Monster_SavingThrows (MonsterID INTEGER, SavingThrowID INTEGER, Value INTEGER NOT NULL, PRIMARY KEY (MonsterID, SavingThrowID))"  # The new join table
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
    hp_avg = data.get("hit_points", 0)
    hp_formula_str = data.get("hit_dice", "")

    # We get the monster's Constitution FIRST, as it is the source of the modifier.
    constitution = data.get("constitution", 10)

    # My function will correctly parse the dice. We no longer care about a modifier from the string.
    num_dice, die_type, _ = parse_hp_formula(hp_formula_str)

    # THE TRUE STRATEGY: Calculate the modifier from the monster's core vitality.
    # This is the value you have been seeking.
    con_modifier = (constitution - 10) // 2
    total_hp_modifier = con_modifier * num_dice
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
            Name, ArmorClass, HitPoints_Avg, HitPoints_Formula, HitPoints_NumDice, 
            HitPoints_DieType, HitPoints_Modifier, Speed,
            Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma,
            Languages
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            monster_name,
            ac,
            hp_avg,
            hp_formula_str,
            num_dice,
            die_type,
            total_hp_modifier,  # <-- Here, we insert the CORRECTLY CALCULATED value.
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

                cursor.execute(
                    "INSERT OR IGNORE INTO Monster_SavingThrows (MonsterID, SavingThrowID, Value) VALUES (?, ?, ?)",
                    (monster_id, st_id, prof_value),
                )

    damage_types_map = lookup_data.get("damagetypes", {})

    process_damage_list(
        cursor,
        monster_id,
        data.get("damage_vulnerabilities", []),
        "Monster_DamageVulnerabilities",
        damage_types_map,
    )
    process_damage_list(
        cursor,
        monster_id,
        data.get("damage_resistances", []),
        "Monster_DamageResistances",
        damage_types_map,
    )
    process_damage_list(
        cursor,
        monster_id,
        data.get("damage_immunities", []),
        "Monster_DamageImmunities",
        damage_types_map,
    )

    # --- Parse and Store Senses ---
    senses_data = data.get("senses", {})
    senses_map = lookup_data.get("senses", {})

    if senses_data:
        print("  -> Analyzing senses...")
        for sense_name_raw, sense_value in senses_data.items():
            # Normalize the sense name from the monster data (e.g., "passive_perception")
            # to match the lookup table (e.g., "passive perception")
            sense_name = sense_name_raw.replace("_", " ").lower()

            if sense_name in senses_map:
                sense_id = senses_map[sense_name]
                # The value can be an integer or a string, so convert to string to be safe
                value_str = str(sense_value)

                cursor.execute(
                    "INSERT OR IGNORE INTO Monster_Senses (MonsterID, SenseID, Value) VALUES (?, ?, ?)",
                    (monster_id, sense_id, value_str),
                )

    # --- Parse and Store Condition Immunities ---
    condition_immunities = data.get("condition_immunities", [])
    conditions_map = lookup_data.get("conditions", {})

    if condition_immunities:
        print("  -> Analyzing condition immunities...")
        for cond in condition_immunities:
            cond_name = cond.get("name", "").lower()
            if cond_name in conditions_map:
                cond_id = conditions_map[cond_name]
                cursor.execute(
                    "INSERT OR IGNORE INTO Monster_ConditionImmunities (MonsterID, ConditionID) VALUES (?, ?)",
                    (monster_id, cond_id),
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

            cursor.execute(
                "INSERT INTO Abilities (MonsterID, Name, Description, AbilityType) VALUES (?, ?, ?, ?)",
                (monster_id, ability_name, ability_desc, ability_type),
            )
            ability_id = cursor.lastrowid

            # --- Parse Ability Usage ---
            usage_data = ability_data.get("usage")
            if usage_data:
                usage_type = usage_data.get("type")
                if usage_type == "per day":
                    uses_max = usage_data.get("times")
                    if uses_max:
                        print(f"  -> Found usage: {uses_max}/Day")
                        cursor.execute(
                            "INSERT INTO Ability_Usage (AbilityID, UsageType, UsesMax) VALUES (?, ?, ?)",
                            (ability_id, "Per Day", uses_max),
                        )
                elif usage_type in ["recharge on roll", "recharge after rest"]:
                    recharge_value = usage_data.get("min_value")
                    if recharge_value:
                        print(f"  -> Found usage: Recharge {recharge_value}+")
                        cursor.execute(
                            "INSERT INTO Ability_Usage (AbilityID, UsageType, RechargeValue) VALUES (?, ?, ?)",
                            (ability_id, "Recharge", recharge_value),
                        )

            if ability_name == "Spellcasting":
                spells_found = 0
                lines = ability_desc.split("\n")

                # Regex to find spell slots, e.g., "1st level (4 slots): ..."
                slot_pattern = re.compile(r"(\d+)(?:st|nd|rd|th) level \((\d+) slots\)")

                for line in lines:
                    # First, check for spell slots on this line
                    slot_match = slot_pattern.search(line)
                    if slot_match:
                        spell_level = int(slot_match.group(1))
                        num_slots = int(slot_match.group(2))
                        print(
                            f"  -> Found {num_slots} slots for level {spell_level} spells."
                        )
                        cursor.execute(
                            "INSERT OR IGNORE INTO Monster_SpellSlots (MonsterID, SpellLevel, Slots) VALUES (?, ?, ?)",
                            (monster_id, spell_level, num_slots),
                        )

                    # Then, parse the spell names from the line
                    if ":" in line:
                        parts = line.split(":", 1)
                        if len(parts) == 2:
                            spell_list_str = parts[1]
                            spell_names = [
                                spell.strip()
                                .replace(".", "")
                                .replace("*", "")  # Also remove asterisks
                                for spell in spell_list_str.split(",")
                            ]
                            for spell_name in spell_names:
                                if not spell_name:
                                    continue

                                # Clean up spell name further if needed, e.g. "(self only)"
                                spell_name = re.sub(r"\(.*?\)", "", spell_name).strip()

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
                                        "INSERT OR IGNORE INTO Monster_Spells (MonsterID, SpellID) VALUES (?, ?)",
                                        (monster_id, spell_id),
                                    )
                                else:
                                    print(
                                        f"  -> WARNING: Could not find spell '{spell_name}' in the grimoire."
                                    )

                if spells_found > 0:
                    print(f"  -> Successfully linked {spells_found} spells.")


def parse_hp_formula(formula):
    """
    Dissects the hit point formula string into its component parts.
    This final, robust version uses a simpler pattern and cleans the input.

    Args:
        formula (str): The string representing the hit point formula (e.g., ' 2d8 + 4 ').

    Returns:
        tuple: A tuple containing (num_dice, die_type, modifier).
               Returns (0, 0, 0) if the formula cannot be parsed.
    """
    # First, a commander must ensure their intelligence is clean.
    if not formula:
        return 0, 0, 0
    cleaned_formula = formula.strip()

    # A simpler, more direct pattern. It captures the entire modifier string.
    pattern = re.compile(r"(\d+)d(\d+)\s*([+-]\s*\d+)?")
    match = pattern.match(cleaned_formula)

    if match:
        num_dice = int(match.group(1))
        die_type = int(match.group(2))

        modifier = 0
        modifier_str = match.group(3)  # This will be like '+ 36', '- 3', or None

        if modifier_str:
            # Remove spaces from the modifier string and convert to an integer
            modifier = int(modifier_str.replace(" ", ""))

        return num_dice, die_type, modifier

    # Fallback for formulas that are just a number (e.g., "7")
    try:
        only_number = int(cleaned_formula)
        return (0, 0, only_number)
    except (ValueError, TypeError):
        return (0, 0, 0)


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
def process_damage_list(cursor, monster_id, damage_list, table_name, damage_types_map):
    if not damage_list:
        return

    print(f"  -> Analyzing {table_name.replace('_', ' ').lower()}...")

    for item_str in damage_list:
        # Each item in damage_list can contain multiple rules separated by semicolons
        rules = [rule.strip() for rule in item_str.lower().split(";")]

        for rule in rules:
            if not rule:
                continue

            note = ""
            damage_section = rule

            # Find keywords that separate damage types from a condition/note
            match = re.search(
                r"^(.*?)(?:\s+\bfrom\b|\s+\bexcept\b|\s+that\b)(.*)$", rule
            )
            if match:
                damage_section = match.group(1).strip()
                note = rule.replace(damage_section, "").strip()

            # Find all the damage types mentioned in the damage section of the rule
            found_types = []
            # We split by ',' or 'and' to find individual damage type names
            potential_types = re.split(r",|\s+and\s+", damage_section)
            for p_type in potential_types:
                # Clean up the potential type name
                p_type = p_type.strip()
                if p_type in damage_types_map:
                    found_types.append(p_type)

            # If we found specific types, add them to the database with the note
            if found_types:
                for damage_type_name in found_types:
                    damage_type_id = damage_types_map[damage_type_name]

                    # For Resistances, we store the note. For others, we don't.
                    if "Resistances" in table_name:
                        cursor.execute(
                            f"INSERT OR IGNORE INTO {table_name} (MonsterID, DamageTypeID, Note) VALUES (?, ?, ?)",
                            (monster_id, damage_type_id, note),
                        )
                    else:
                        cursor.execute(
                            f"INSERT OR IGNORE INTO {table_name} (MonsterID, DamageTypeID) VALUES (?, ?)",
                            (monster_id, damage_type_id),
                        )
            # If we didn't find specific types, the whole rule might be a note.
            # This happens with things like "bludgeoning, piercing, and slashing from nonmagical attacks"
            # where the damage types are not perfectly separated.
            # Let's add a fallback for this common case.
            else:
                # Check for the big three physical types in the original rule string
                physical_types_present = []
                if "bludgeoning" in rule:
                    physical_types_present.append("bludgeoning")
                if "piercing" in rule:
                    physical_types_present.append("piercing")
                if "slashing" in rule:
                    physical_types_present.append("slashing")

                if physical_types_present:
                    # The note is the full rule string
                    full_note = rule
                    for damage_type_name in physical_types_present:
                        damage_type_id = damage_types_map[damage_type_name]
                        if "Resistances" in table_name:
                            cursor.execute(
                                f"INSERT OR IGNORE INTO {table_name} (MonsterID, DamageTypeID, Note) VALUES (?, ?, ?)",
                                (monster_id, damage_type_id, full_note),
                            )
                        else:
                            cursor.execute(
                                f"INSERT OR IGNORE INTO {table_name} (MonsterID, DamageTypeID) VALUES (?, ?)",
                                (monster_id, damage_type_id),
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
        "Senses": "SenseID",
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
