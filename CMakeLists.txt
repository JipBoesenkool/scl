cmake_minimum_required(VERSION 3.15)
project(scl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# --- 2. cLib Library ---
add_subdirectory(src)

# 3. Library Logic Tests (Using the helper)
#if(BUILD_TESTS)
enable_testing()
add_subdirectory(tests)

# --- 4. Generated Demos ---
#if(BUILD_EXAMPLES)
add_subdirectory(demo)
