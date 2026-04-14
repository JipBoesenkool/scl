cmake_minimum_required(VERSION 3.15)
project(ZoneDemo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 3. Define the Debugger Executable ---
# Automatically collect all .c and .cpp files from the c/src directory
file(GLOB_RECURSE CPP_CORE_SOURCES "imgui/*.cpp")
file(GLOB_RECURSE CPP_CORE_HEADERS "imgui/*.hpp")

add_executable(${PROJECT_NAME} 
    main.cpp
    ${CPP_CORE_SOURCES}
    ${CPP_CORE_HEADERS}
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
set_target_properties(${PROJECT_NAME} PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
)
target_include_directories(${PROJECT_NAME} PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
  cLib 
  SDL3::SDL3
)

# --- 4. Windows DLL Copying (Only for UI App) ---
if(WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:SDL3::SDL3-shared>
      $<TARGET_FILE_DIR:${PROJECT_NAME}>
  )
endif()
