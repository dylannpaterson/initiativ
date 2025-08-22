import sqlite3
import requests
import re
import json

# --- Configuration ---
DATABASE_NAME = "data/initiativ.sqlite"
API_BASE_URL = "https://www.dnd5eapi.co/api/"


def setup_database():
    """Establishes the Archives, creating tables only if they don't exist."""
    print("Inspecting the Archives...")
    conn = sqlite3.connect(DATABASE_NAME)
    cursor = conn.cursor()

    # This non-destructive setup is now the default
    print("Ensuring all necessary tables exist...")
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Monsters (
        MonsterID INTEGER PRIMARY KEY AUTOINCREMENT,
        Name TEXT NOT NULL UNIQUE,
        ArmorClass INTEGER,
        HitPoints_Avg INTEGER,
        HitPoints_Formula TEXT,
        Speed TEXT
    )
    """
    )
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Abilities (
        AbilityID INTEGER PRIMARY KEY AUTOINCREMENT,
        MonsterID INTEGER,
        Name TEXT NOT NULL,
        Description TEXT,
        AbilityType TEXT,
        FOREIGN KEY (MonsterID) REFERENCES Monsters (MonsterID)
    )
    """
    )
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Ability_Effects (
        EffectID INTEGER PRIMARY KEY AUTOINCREMENT,
        AbilityID INTEGER,
        EffectType TEXT,
        AttackBonus INTEGER,
        DamageDice TEXT,
        DamageType TEXT,
        SaveDC INTEGER,
        SaveAttribute TEXT,
        FOREIGN KEY (AbilityID) REFERENCES Abilities (AbilityID)
    )
    """
    )
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Ability_Usage (
        UsageID INTEGER PRIMARY KEY AUTOINCREMENT,
        AbilityID INTEGER,
        UsageType TEXT,
        UsesMax INTEGER,
        RechargeValue INTEGER,
        FOREIGN KEY (AbilityID) REFERENCES Abilities (AbilityID)
    )
    """
    )
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Spells (
        SpellID INTEGER PRIMARY KEY AUTOINCREMENT,
        Name TEXT NOT NULL UNIQUE COLLATE NOCASE,
        Description TEXT,
        Level INTEGER,
        CastingTime TEXT,
        Range TEXT,
        Components TEXT,
        Duration TEXT
    )
    """
    )
    cursor.execute(
        """
    CREATE TABLE IF NOT EXISTS Monster_Spells (
        MonsterSpellID INTEGER PRIMARY KEY AUTOINCREMENT,
        MonsterID INTEGER,
        SpellID INTEGER,
        FOREIGN KEY (MonsterID) REFERENCES Monsters (MonsterID),
        FOREIGN KEY (SpellID) REFERENCES Spells (SpellID)
    )
    """
    )

    conn.commit()
    conn.close()
    print("Archives are properly structured.")


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


def parse_and_store_monster(monster_slug, conn):
    """Checks if a monster exists before fetching and parsing its data."""
    cursor = conn.cursor()
    monster_name_from_slug = monster_slug.replace("-", " ").title()

    cursor.execute(
        "SELECT MonsterID FROM Monsters WHERE Name = ?", (monster_name_from_slug,)
    )
    result = cursor.fetchone()

    if result:
        print(
            f"\n--- Dossier for '{monster_name_from_slug}' already in Archives. Skipping. ---"
        )
        return

    print(f"\n--- Interrogating dossier for: {monster_slug.upper()} ---")
    # (The rest of the function remains the same as the last version)
    try:
        response = requests.get(f"{API_BASE_URL}monsters/{monster_slug}")
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        print(f"  -> Mission Aborted: Could not retrieve intelligence. {e}")
        return

    monster_name = data.get("name")
    ac = data.get("armor_class", [{}])[0].get("value", 10)
    hp_avg = data.get("hit_points")
    hp_formula = data.get("hit_points_dice")
    speed = json.dumps(data.get("speed", {}))

    cursor.execute(
        "INSERT INTO Monsters (Name, ArmorClass, HitPoints_Avg, HitPoints_Formula, Speed) VALUES (?, ?, ?, ?, ?)",
        (monster_name, ac, hp_avg, hp_formula, speed),
    )
    monster_id = cursor.lastrowid
    print(f"  -> Stored '{monster_name}' with MonsterID: {monster_id}")

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


if __name__ == "__main__":
    # Step 1: Ensure the Archives are structured correctly.
    setup_database()

    # Step 2: Open a single connection for the entire operation.
    conn = sqlite3.connect(DATABASE_NAME)

    # Step 3: Populate the spell grimoire with any missing spells.
    populate_spell_grimoire(conn)

    # Step 4: Launch the full reconnaissance of all monster dossiers.
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
            parse_and_store_monster(monster_slug, conn)

    except requests.exceptions.RequestException as e:
        print(f"  -> CRITICAL FAILURE: Could not retrieve monster index. {e}")

    # Step 5: Commit all gathered intelligence and close the connection.
    conn.commit()
    conn.close()

    print("\nCampaign Phase 0: Complete. The Archives are now fully stocked.")
