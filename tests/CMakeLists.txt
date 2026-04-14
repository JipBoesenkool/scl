cmake_minimum_required(VERSION 3.15)
project(cLibTests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# -- dependency
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
set(CATCH_INSTALL_DOCS OFF CACHE INTERNAL "")
set(CATCH_INSTALL_EXTRAS OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(Catch2)

# --- 4. Testing Suite ---

# Helper function to reduce boilerplate for simple library tests
function(add_clib_test TEST_NAME SRC_FILE)
    add_executable(${TEST_NAME} ${SRC_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE cLib Catch2::Catch2WithMain)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# C. Library Logic Tests (Using the helper)
add_subdirectory(c)    # C API with NPM something
add_subdirectory(cpp)  # C++ API
