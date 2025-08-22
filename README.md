# initiativ

*initiativ is a cross-platform application designed for Dungeon Masters to efficiently prepare and run Dungeons & Dragons 5th Edition combat encounters. It combines a robust database of creatures with an intuitive interface for tracking initiative, resources, and the overall flow of battle.*

---

## Phase 0: The Archives (Data Acquisition & Parsing)

> **Objective:** To create the logistical backbone of the application. This phase focuses on building a standalone "Scribe's Tool" to construct and populate a comprehensive, local **SQLite** database of monster stat blocks.

**Key Tasks:**
* **Schema Design:** Finalize and implement the SQLite database schema for `Monsters`, `Abilities`, `Ability_Effects`, and `Ability_Usage`.
* **Parser Engine Development:** Build the core parsing engine using regular expressions, designed to be modular and capable of deciphering raw text stat blocks.
* **Establish the Proving Grounds:** Create a comprehensive test suite with stat block examples from various online sources (Roll20, D&D Beyond, etc.) to ensure the parser is accurate and to prevent regressions.
* **Build the Scribe's Tool Interface:** Create two interfaces for the parser engine:
    * **Bulk Importer:** To perform the initial, large-scale population from the D&D 5e API.
    * **Custom Scribe:** A simple command-line tool for adding or updating individual monsters from pasted text.

---

## Phase 1: The Foundation (Core Application Setup)

> **Objective:** To establish a minimal, cross-platform **C++** application that can communicate with the Archives and serve as the foundation for the user interface.

**Key Tasks:**
* **Project Scaffolding:** Set up the C++ project structure with a **CMake** build system for portability across Windows, macOS, and Linux.
* **Library Integration:** Integrate and test the core libraries: **SDL2** for window and event management, and a **SQLite** library for database interaction.
* **Core Data Structures:** Implement the main C++ classes (`Combatant`, `Encounter`, etc.) and the logic to load creature data from the SQLite database created in Phase 0.
* **Basic Proof-of-Concept:** Create a simple command-line version to prove that encounters can be loaded and combatants can be manipulated before any GUI work begins.

---

## Phase 2: The Command Tent (GUI Implementation)

> **Objective:** To build the graphical user interface where the Dungeon Master will command the flow of battle.

**Key Tasks:**
* **GUI Framework Integration:** Integrate **Dear ImGui** into the SDL2 application to serve as the GUI framework.
* **View Construction:** Design and implement the primary UI components:
    * **Encounter Setup View:** For selecting creatures from the database to build an encounter.
    * **Combat Tracker View:** The main display, showing initiative order, HP, conditions, and the current turn.
    * **Unit Detail Pane:** A view for displaying the full stat block of a selected creature.
* **Interactivity:** Implement the logic for managing the encounter through the GUI: inputting initiative, dealing damage, tracking resources, and advancing turns.
* **Live Combat Log:** Display a real-time log of actions and events as they occur during the encounter.

---

## Phase 3: Reinforcements (Advanced Features & Polish)

> **Objective:** To add advanced functionality, refine the user experience, and prepare the application for distribution.

**Key Tasks:**
* **Persistent Logging:** Implement the functionality to save the combat log to a text file for later review.
* **Application Polishing:** Refine the user interface, add quality-of-life features, and ensure stable performance.
* **Packaging:** Create distributable packages of the application for each target operating system.
* **(Tertiary Objective) The Summoned Oracle:** Investigate and implement the optional, expert-level **LLM Parser** as a fallback mechanism for the Scribe's Tool, contingent on user-provided API keys.
