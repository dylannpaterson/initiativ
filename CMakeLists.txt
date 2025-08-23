cmake_minimum_required(VERSION 3.10)
project(initiativ CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fetch and include SQLiteCpp as a third-party dependency ---
include(FetchContent)
FetchContent_Declare(
    SQLiteCpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG 3.3.0
)
FetchContent_MakeAvailable(SQLiteCpp)

# --- Find the SDL2 package and the OpenGL library on the system ---
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# --- Define ImGui as a separate library target with its source files and backends ---
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

# --- Set the include directories for the ImGui library ---
target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
    ${SDL2_INCLUDE_DIRS}
)

# --- Define the executable target for the project ---
add_executable(initiativ src/main.cpp)

# --- Set the project's include directories ---
target_include_directories(initiativ PUBLIC
    src/
    ${SQLITECPP_SOURCE_DIR}/include
)

# --- Link the necessary libraries for the project ---
target_link_libraries(initiativ PRIVATE
    SQLiteCpp
    SDL2::SDL2main
    SDL2::SDL2
    imgui
    OpenGL::GL
)

# --- Optional: Add install targets for deployment ---
install(TARGETS initiativ DESTINATION bin)