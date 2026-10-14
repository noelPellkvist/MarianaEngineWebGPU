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
  # Only what Player.json uses is packaged with the engine's resources: Player.json itself, the
  # pipeline if the project has its own, and each prefab (a .gltf with the folder holding its
  # buffers and images). Editing Player.json re-runs this.
  set(player_json "${MARIANA_PLAYER_PROJECT}/Player.json")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${player_json}")
  file(READ "${player_json}" player_config)
  set(player_files "${player_json}")

  string(JSON pipeline ERROR_VARIABLE json_error GET "${player_config}" pipeline)
  if(NOT json_error AND EXISTS "${MARIANA_PLAYER_PROJECT}${pipeline}")
    list(APPEND player_files "${MARIANA_PLAYER_PROJECT}${pipeline}")
  endif()

  string(JSON spawn_count ERROR_VARIABLE json_error LENGTH "${player_config}" spawn)
  if(NOT json_error AND spawn_count GREATER 0)
    math(EXPR last_spawn "${spawn_count} - 1")
    foreach(i RANGE ${last_spawn})
      string(JSON prefab GET "${player_config}" spawn ${i} prefab)
      set(prefab_file "${MARIANA_PLAYER_PROJECT}${prefab}")
      if(prefab_file MATCHES "\\.gltf$")
        get_filename_component(prefab_file "${prefab_file}" DIRECTORY)
      endif()
      list(APPEND player_files "${prefab_file}")
    endforeach()
  endif()
  list(REMOVE_DUPLICATES player_files)

  # Kept out of bin/, which is what the Pages workflow publishes (the editor).
  set_target_properties(MarianaPlayer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/player")
  mariana_web_app(MarianaPlayer
    OUTPUT_NAME player
    SHELL "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    PRELOAD ${player_files}
  )
endif()
