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

# ==========================================
# FIX: Force CMake to find Geode in CI Environments
# ==========================================
if(DEFINED ENV{GEODE_SDK})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{GEODE_SDK}")
else()
    # Default directory where Geode CLI installs the SDK on Linux / GitHub runners
    list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.geode/sdk")
endif()

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
