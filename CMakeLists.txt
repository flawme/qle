cmake_minimum_required(VERSION 3.14)
project(QLE VERSION 0.1.6)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Wall Wextra for code quality
add_compile_options(-Wall -Wextra)

# Include directories
include_directories(src)

# Source files
file(GLOB_RECURSE SRC_FILES src/*.cpp)
list(APPEND SRC_FILES "${CMAKE_SOURCE_DIR}/src/adapters/sqlite/sqlite3.c")

# Remove main from SRC_FILES if building tests
list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/main.cpp")

# Create library
add_library(qle_core ${SRC_FILES})
target_compile_definitions(qle_core PUBLIC QLE_HAVE_SQLITE3)

# Threading and dl required for native SQLite
find_package(Threads REQUIRED)
target_link_libraries(qle_core PUBLIC Threads::Threads dl)

# Main executable
add_executable(qle src/main.cpp)
target_link_libraries(qle PRIVATE qle_core)

enable_testing()
add_subdirectory(tests)
