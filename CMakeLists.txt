cmake_minimum_required(VERSION 3.25)
project(GhostGuide VERSION 1.1.2)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFMT_USE_CONSTEVAL=0")
add_compile_definitions(FMT_USE_CONSTEVAL=0)
add_compile_definitions(FMT_CONSTEVAL=)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# =================================================================
# FIX: Comprehensive search grid for Geode SDK on CI environments
# =================================================================
list(APPEND CMAKE_PREFIX_PATH 
    "$ENV{GEODE_SDK}"
    "$ENV{HOME}/.local/share/Geode/sdk"
    "$ENV{HOME}/.geode/sdk"
    "$ENV{HOME}/Documents/Geode"
)

find_package(Geode REQUIRED)

# Add all source files inside src (recursively)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Modern Geode Mod Target
add_geode_mod(
    joshi1549m.ghost_guide
    VERSION 1.1.2
    SOURCES ${SOURCES}
)

setup_geode_mod(joshi1549m.ghost_guide VERSION 1.1.2 GEODE 5.7.1)
