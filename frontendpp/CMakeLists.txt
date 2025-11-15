cmake_minimum_required(VERSION 3.16)
project(FrontendPP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include our custom cmake attributes system
add_subdirectory(frontendpp/cmake)

# Add build directory to include path for generated headers
include_directories(${CMAKE_BINARY_DIR})

# Find required packages with better error handling
find_package(PkgConfig REQUIRED)

# Use pkg_check_modules for all dependencies
pkg_check_modules(OPENSSL REQUIRED openssl)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_INCLUDE_DIRS}")
    message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "OpenSSL is required but not found. Please install libssl-dev")
endif()

# Find SQLite3
pkg_check_modules(SQLITE3 QUIET sqlite3)
if(NOT SQLITE3_FOUND)
    # Try find_package as fallback
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        message(STATUS "Found SQLite3 via find_package: ${SQLite3_INCLUDE_DIR}")
        set(SQLITE3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIR})
        set(SQLITE3_LIBRARIES ${SQLite3_LIBRARY})
        set(SQLITE3_FOUND TRUE)
    else()
        # Try to find system sqlite3 library directly
        find_library(SQLITE3_SYSTEM_LIBRARY sqlite3)
        if(SQLITE3_SYSTEM_LIBRARY)
            message(STATUS "Found system SQLite3 library: ${SQLITE3_SYSTEM_LIBRARY}")
            set(SQLITE3_LIBRARIES ${SQLITE3_SYSTEM_LIBRARY})
            set(SQLITE3_INCLUDE_DIRS "/usr/include")
            set(SQLITE3_FOUND TRUE)
        else()
            message(FATAL_ERROR "SQLite3 is required but not found. Please install libsqlite3-dev")
        endif()
    endif()
else()
    message(STATUS "Found SQLite3: ${SQLITE3_INCLUDE_DIRS}")
endif()

# Find libmicrohttpd
pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
if(NOT MICROHTTPD_FOUND)
    # Try find_package as fallback
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(MICROHTTPD REQUIRED libmicrohttpd)
    else()
        message(FATAL_ERROR "libmicrohttpd is required but not found. Please install libmicrohttpd-dev")
    endif()
else()
    message(STATUS "Found libmicrohttpd: ${MICROHTTPD_INCLUDE_DIRS}")
endif()

# Find nlohmann_json
pkg_check_modules(NLOHMANN_JSON QUIET nlohmann_json)
if(NOT NLOHMANN_JSON_FOUND)
    # Try find_package as fallback
    find_package(nlohmann_json QUIET)
    if(nlohmann_json_FOUND)
        message(STATUS "Found nlohmann_json via find_package")
        set(NLOHMANN_JSON_INCLUDE_DIRS ${nlohmann_json_INCLUDE_DIRS})
        set(NLOHMANN_JSON_LIBRARIES ${nlohmann_json_LIBRARIES})
        set(NLOHMANN_JSON_FOUND TRUE)
    else()
        # Use header-only fallback
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/nlohmann/json.hpp")
            message(STATUS "Using header-only nlohmann_json fallback")
            set(NLOHMANN_JSON_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")
            set(NLOHMANN_JSON_FOUND TRUE)
        else()
            message(FATAL_ERROR "nlohmann_json is required but not found. Please install libnlohmann-json-dev")
        endif()
    endif()
else()
    message(STATUS "Found nlohmann_json: ${NLOHMANN_JSON_INCLUDE_DIRS}")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

# Add jwt-cpp library
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/jwtpp/CMakeLists.txt")
    add_subdirectory(thirdparty/jwtpp)
    message(STATUS "Added jwt-cpp library")
else()
    message(FATAL_ERROR "jwt-cpp submodule not found. Please run: git submodule update --init --recursive")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/http_server.cpp
    src/auth_handler.cpp
    src/config_manager.cpp
    src/jwt_manager.cpp
    src/file_handler.cpp
    src/logger.cpp
)

# Header files
set(HEADERS
    include/http_server.h
    include/auth_handler.h
    include/config_manager.h
    include/jwt_manager.h
    include/file_handler.h
    include/logger.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries based on what was found
target_link_libraries(${PROJECT_NAME} 
    ${OPENSSL_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${MICROHTTPD_LIBRARIES}
    jwt-cpp
    pthread
)

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${OPENSSL_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${MICROHTTPD_INCLUDE_DIRS}
    ${NLOHMANN_JSON_INCLUDE_DIRS}
)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -O2)

# Add definitions for found packages
target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENSSL HAVE_SQLITE3 HAVE_MICROHTTPD)

# Copy config file to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/server.json ${CMAKE_BINARY_DIR}/config/server.json COPYONLY)

# Print summary
message(STATUS "")
message(STATUS "Frontend++ Configuration Summary:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  OpenSSL: ${OPENSSL_FOUND}")
message(STATUS "  SQLite3: ${SQLITE3_FOUND}")
message(STATUS "  libmicrohttpd: ${MICROHTTPD_FOUND}")
message(STATUS "  nlohmann_json: ${NLOHMANN_JSON_FOUND}")
message(STATUS "")
