cmake_minimum_required(VERSION 3.16)
project(network-priority)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${NLOHMANN_JSON_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIRS})

# Add parent include directories for ThreadManager and DatabaseManager
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ur-threadder-api/cpp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ur-threadder-api/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# Source files
set(SOURCES
    src/NetworkPriorityManager.cpp
)

# Header files
set(HEADERS
    include/NetworkPriorityManager.h
)

# Create static library
add_library(network_priority STATIC ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(network_priority
    ${NLOHMANN_JSON_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    threadmanager_cpp
    pthread
)

# Add definitions
target_compile_definitions(network_priority PRIVATE HAVE_NLOHMANN_JSON HAVE_SQLITE3)

# Export include directories and libraries for parent project
set(NETWORK_PRIORITY_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include ${NLOHMANN_JSON_INCLUDE_DIRS} ${SQLITE3_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../ur-threadder-api/cpp/include ${CMAKE_CURRENT_SOURCE_DIR}/../ur-threadder-api/include ${CMAKE_CURRENT_SOURCE_DIR}/../../include PARENT_SCOPE)
set(NETWORK_PRIORITY_LIBRARIES network_priority PARENT_SCOPE)

# Compiler flags
target_compile_options(network_priority PRIVATE -Wall -Wextra -O2)
