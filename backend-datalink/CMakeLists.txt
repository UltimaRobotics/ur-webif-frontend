cmake_minimum_required(VERSION 3.16)
project(backend-datalink)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add ur-threadder-api as subdirectory
add_subdirectory(thirdparty/ur-threadder-api)

# Add system-data as subdirectory
add_subdirectory(thirdparty/system-data)

# Add network-priority as subdirectory
add_subdirectory(thirdparty/network-priority)

# Add ur-rpc-template as subdirectory
add_subdirectory(thirdparty/ur-rpc-template)

# Find SQLite3
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 QUIET sqlite3)
if(NOT SQLITE3_FOUND)
    # Try find_package as fallback
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        message(STATUS "Found SQLite3 via find_package")
        set(SQLITE3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIRS})
        set(SQLITE3_LIBRARIES ${SQLite3_LIBRARIES})
    else()
        message(FATAL_ERROR "SQLite3 is required but not found. Please install libsqlite3-dev")
    endif()
else()
    message(STATUS "Found SQLite3 via pkg-config")
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

# Find websocketpp
pkg_check_modules(WEBSOCKETPP QUIET websocketpp)
if(NOT WEBSOCKETPP_FOUND)
    # Try find_package as fallback
    find_package(websocketpp QUIET)
    if(websocketpp_FOUND)
        message(STATUS "Found websocketpp via find_package")
        set(WEBSOCKETPP_INCLUDE_DIRS ${websocketpp_INCLUDE_DIRS})
        set(WEBSOCKETPP_FOUND TRUE)
    else()
        # Use header-only fallback
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/websocketpp/websocketpp/server.hpp")
            message(STATUS "Using header-only websocketpp fallback")
            set(WEBSOCKETPP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/websocketpp")
            set(WEBSOCKETPP_FOUND TRUE)
        else()
            message(FATAL_ERROR "websocketpp is required but not found. Please install libwebsocketpp-dev")
        endif()
    endif()
else()
    message(STATUS "Found websocketpp: ${WEBSOCKETPP_INCLUDE_DIRS}")
endif()

# Find OpenSSL (required for websocketpp)
pkg_check_modules(OPENSSL REQUIRED openssl)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_INCLUDE_DIRS}")
    message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "OpenSSL is required but not found. Please install libssl-dev")
endif()

# Check for ASIO (required for websocketpp)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/include/asio.hpp")
    message(STATUS "Using standalone ASIO from thirdparty")
    set(ASIO_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/include")
    set(ASIO_FOUND TRUE)
else()
    message(FATAL_ERROR "ASIO is required but not found. Please ensure ASIO headers are available")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

# Source files
set(SOURCES
    src/main.cpp
    src/websocket_server.cpp
    src/managed_websocket_server.cpp
    src/config_loader.cpp
    src/database_manager.cpp
    src/rpc_client.cpp
)

# Header files
set(HEADERS
    include/managed_websocket_server.h
    include/websocket_server.h
    include/config_loader.h
    include/database_manager.h
    include/rpc_client.h
)

# Create executable
add_executable(backend-datalink ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(backend-datalink 
    ${OPENSSL_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    system_data
    network_priority
    threadmanager
    threadmanager_cpp
    ur-rpc-template
    ur-rpc-cpp
    pthread
)

# Add include directories
target_include_directories(backend-datalink PRIVATE 
    ${OPENSSL_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${NLOHMANN_JSON_INCLUDE_DIRS}
    ${WEBSOCKETPP_INCLUDE_DIRS}
    ${ASIO_INCLUDE_DIRS}
    ${THREADDER_INCLUDE_DIRS}
    ${SYSTEM_DATA_INCLUDE_DIRS}
    ${NETWORK_PRIORITY_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-rpc-template
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-rpc-template/extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-rpc-template/deps/cJSON
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-rpc-template/deps/mqtt-client/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-rpc-template/deps/ur-logger-api
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ur-threadder-api/cpp/include
)

# Compiler flags
target_compile_options(backend-datalink PRIVATE -Wall -Wextra -O2)

# Add definitions for found packages
target_compile_definitions(backend-datalink PRIVATE HAVE_OPENSSL HAVE_NLOHMANN_JSON HAVE_WEBSOCKETPP ASIO_STANDALONE)

# Copy config files to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.json ${CMAKE_BINARY_DIR}/config/config.json COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/rpc_config.json ${CMAKE_BINARY_DIR}/config/rpc_config.json COPYONLY)

# Print summary
message(STATUS "")
message(STATUS "Backend-Datalink Configuration Summary:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  OpenSSL: ${OPENSSL_FOUND}")
message(STATUS "  nlohmann_json: ${NLOHMANN_JSON_FOUND}")
message(STATUS "  websocketpp: ${WEBSOCKETPP_FOUND}")
message(STATUS "  ASIO: ${ASIO_FOUND}")
message(STATUS "")
