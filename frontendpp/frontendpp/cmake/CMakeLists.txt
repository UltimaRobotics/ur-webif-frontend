# CMake configuration for FrontendPP build-time attributes
cmake_minimum_required(VERSION 3.16)

# Generate build timestamp
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC")

# Get git commit hash
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(GIT_COMMIT_HASH "unknown")
endif()

# Get compiler info
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(COMPILER_VERSION "GCC ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMPILER_VERSION "Clang ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(COMPILER_VERSION "MSVC ${CMAKE_CXX_COMPILER_VERSION}")
else()
    set(COMPILER_VERSION "Unknown")
endif()

# Build type configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG_BUILD true)
else()
    set(DEBUG_BUILD false)
endif()

# Feature flags
set(VERBOSE_ENABLED true CACHE BOOL "Enable verbose logging")
set(ENCRYPTION_ENABLED true CACHE BOOL "Enable data encryption")

# Application metadata
set(APP_NAME "FrontendPP" CACHE STRING "Application name")
set(APP_VERSION "2.0.0" CACHE STRING "Application version")
set(APP_DESCRIPTION "Production C++ Web Server with Enhanced Authentication" CACHE STRING "Application description")

# Default credentials (for initial database creation)
set(DEFAULT_ADMIN_USERNAME "admin" CACHE STRING "Default admin username")
set(DEFAULT_ADMIN_PASSWORD "admin123" CACHE STRING "Default admin password")
set(DEFAULT_ADMIN_EMAIL "admin@frontendpp.local" CACHE STRING "Default admin email")

# Generate default encryption key (hex-encoded AES-256 key)
# This is a 64-character hex string (32 bytes)
set(DEFAULT_ENCRYPTION_KEY "2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190abc" CACHE STRING "Default AES-256 encryption key (hex)")

# Generate default JWT secret (will be overridden at runtime)
set(DEFAULT_JWT_SECRET "your-256-bit-secret-key-change-this-in-production" CACHE STRING "Default JWT secret")

# Configure the attributes header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/attributes.hpp.in
    ${CMAKE_BINARY_DIR}/frontendpp/cmake/attributes.hpp
    @ONLY
)

# Create include directory for generated headers
include_directories(${CMAKE_BINARY_DIR})

# Add the configured header to the build
set(ATTRIBUTES_HEADER ${CMAKE_BINARY_DIR}/frontendpp/cmake/attributes.hpp)
message(STATUS "Configured build attributes: ${ATTRIBUTES_HEADER}")
message(STATUS "Build timestamp: ${BUILD_TIMESTAMP}")
message(STATUS "Git commit: ${GIT_COMMIT_HASH}")
message(STATUS "Default admin: ${DEFAULT_ADMIN_USERNAME}/${DEFAULT_ADMIN_PASSWORD}")
