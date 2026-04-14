cmake_minimum_required(VERSION 3.15)
project(cLib LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. CORE C LIBRARY (cLib-c) ---
# Automatically collect all .c and .cpp files from the c/src directory
file(GLOB_RECURSE C_CORE_SOURCES "c/src/*.c")
add_library(cLib-c STATIC ${C_CORE_SOURCES})
# Force C++ compilation for the C core (to support NSDMI in headers)
# TODO: remove NSDMI
set_source_files_properties(${C_CORE_SOURCES} PROPERTIES LANGUAGE CXX)

target_include_directories(cLib-c PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/c/include
)

# --- 2. C++ WRAPPER LIBRARY (cLib-cpp) ---
file(GLOB_RECURSE CPP_CORE_SOURCES "cpp/src/*.cpp")
add_library(cLib-cpp STATIC ${CPP_CORE_SOURCES})
target_include_directories(cLib-cpp PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include
)
# Transitive linking: cLib-cpp users get cLib-c headers and binaries automatically
target_link_libraries(cLib-cpp PUBLIC cLib-c)

# Backwards compatibility alias
add_library(cLib ALIAS cLib-cpp)
