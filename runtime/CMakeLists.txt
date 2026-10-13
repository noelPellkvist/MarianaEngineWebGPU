# MarianaPlayer: runs a project without the editor (no UI). It reads the project's Player.json.

file(GLOB_RECURSE MARIANA_PLAYER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_executable(MarianaPlayer ${MARIANA_PLAYER_SOURCES})

target_include_directories(MarianaPlayer PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${PROJECT_SOURCE_DIR}/external/Loaders"
)
target_link_libraries(MarianaPlayer PRIVATE MarianaEngine)

# The project it runs (the sample one for now).
set(MARIANA_PLAYER_PROJECT "${PROJECT_SOURCE_DIR}/projects/Sandbox")
target_compile_definitions(MarianaPlayer PRIVATE MARIANA_PROJECT_DIR="${MARIANA_PLAYER_PROJECT}")

if(EMSCRIPTEN)
  # Kept out of bin/, which is what the Pages workflow publishes (the editor).
  set_target_properties(MarianaPlayer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/player")
  mariana_web_app(MarianaPlayer
    OUTPUT_NAME player
    PRELOAD "${MARIANA_PLAYER_PROJECT}"
  )
endif()
