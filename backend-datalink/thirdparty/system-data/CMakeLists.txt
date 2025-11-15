cmake_minimum_required(VERSION 3.16)
project(system-data)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${NLOHMANN_JSON_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/SystemDataCollector.cpp
)

# Header files
set(HEADERS
    include/SystemDataCollector.h
)

# Create static library
add_library(system_data STATIC ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(system_data
    ${NLOHMANN_JSON_LIBRARIES}
    pthread
)

# Add definitions
target_compile_definitions(system_data PRIVATE HAVE_NLOHMANN_JSON)

# Export include directories and libraries for parent project
set(SYSTEM_DATA_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include ${NLOHMANN_JSON_INCLUDE_DIRS} PARENT_SCOPE)
set(SYSTEM_DATA_LIBRARIES system_data PARENT_SCOPE)

# Compiler flags
target_compile_options(system_data PRIVATE -Wall -Wextra -O2)
