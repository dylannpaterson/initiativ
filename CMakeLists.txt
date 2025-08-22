# Set the minimum required version of CMake and define the project
cmake_minimum_required(VERSION 3.14)
project(initiativ)

# Set the C++ standard we will use
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Include FetchContent for automatic dependency management ---
include(FetchContent)

# --- Find required system libraries ---
# Find the core SDL2 library
find_package(SDL2 REQUIRED)
# Find the core sqlite3 C library, which SQLiteCpp needs
find_package(SQLite3 REQUIRED)

# --- Fetch and configure our C++ specific libraries ---
# Fetch SQLiteCpp, a C++ wrapper for the database
FetchContent_Declare(
  SQLiteCpp
  GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
  GIT_TAG        3.3.0 # Use a specific, stable version
)
FetchContent_MakeAvailable(SQLiteCpp)

# --- Define our application executable ---
add_executable(
  ${PROJECT_NAME}
  src/main.cpp
)

# --- Link all libraries to our application ---
target_include_directories(
  ${PROJECT_NAME} PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${SQLiteCpp_SOURCE_DIR}/include
)

target_link_libraries(
  ${PROJECT_NAME} PRIVATE
  SDL2::SDL2
  SQLite::SQLite3
  SQLiteCpp
)