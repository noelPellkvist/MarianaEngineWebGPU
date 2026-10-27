# MarianaPlayer: runs a project without the editor (no UI). It reads the project's Player.json.

file(GLOB_RECURSE MARIANA_PLAYER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_executable(MarianaPlayer ${MARIANA_PLAYER_SOURCES})

target_include_directories(MarianaPlayer PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${PROJECT_SOURCE_DIR}/external/Loaders"
)
target_link_libraries(MarianaPlayer PRIVATE MarianaEngine MarianaPhysics MarianaScripting)

# The project it runs (the sample one for now).
set(MARIANA_PLAYER_PROJECT "${PROJECT_SOURCE_DIR}/projects/Sandbox")
target_compile_definitions(MarianaPlayer PRIVATE MARIANA_PROJECT_DIR="${MARIANA_PLAYER_PROJECT}")

if(EMSCRIPTEN)
  # Only what Player.json uses is packaged with the engine's resources: Player.json itself, the
  # project's Physics.json if it has one, the pipeline if the project has its own, the scene and the
  # files it lists as its dependencies, and each prefab (a .gltf with the folder holding its buffers
  # and images; a .glb with its extracted materials, if any). Editing Player.json or the scene re-runs
  # this.
  set(player_json "${MARIANA_PLAYER_PROJECT}/Player.json")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${player_json}")
  file(READ "${player_json}" player_config)
  set(player_files "${player_json}")

  # A model with what it needs: a .gltf's whole folder, a .glb and its extracted materials.
  function(mariana_player_add_file resource)
    set(path "${MARIANA_PLAYER_PROJECT}${resource}")
    if(NOT EXISTS "${path}")
      return() # an engine resource (always packaged) or a missing file
    endif()
    set(files "${player_files}")
    if(path MATCHES "\\.gltf$")
      get_filename_component(path "${path}" DIRECTORY)
    elseif(path MATCHES "\\.glb$")
      get_filename_component(dir "${path}" DIRECTORY)
      get_filename_component(name "${path}" NAME_WE)
      if(IS_DIRECTORY "${dir}/${name}.materials")
        list(APPEND files "${dir}/${name}.materials")
      endif()
    endif()
    list(APPEND files "${path}")
    set(player_files "${files}" PARENT_SCOPE)
  endfunction()

  string(JSON scene_path ERROR_VARIABLE json_error GET "${player_config}" scene)
  if(NOT json_error AND EXISTS "${MARIANA_PLAYER_PROJECT}${scene_path}")
    set(scene_file "${MARIANA_PLAYER_PROJECT}${scene_path}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${scene_file}")
    list(APPEND player_files "${scene_file}")
    file(READ "${scene_file}" scene_json)
    string(JSON dependency_count ERROR_VARIABLE json_error LENGTH "${scene_json}" dependencies)
    if(NOT json_error AND dependency_count GREATER 0)
      math(EXPR last_dependency "${dependency_count} - 1")
      foreach(i RANGE ${last_dependency})
        string(JSON dependency GET "${scene_json}" dependencies ${i})
        mariana_player_add_file("${dependency}")
      endforeach()
    endif()
  endif()
  if(EXISTS "${MARIANA_PLAYER_PROJECT}/Physics.json")
    list(APPEND player_files "${MARIANA_PLAYER_PROJECT}/Physics.json")
  endif()

  string(JSON pipeline ERROR_VARIABLE json_error GET "${player_config}" pipeline)
  if(NOT json_error AND EXISTS "${MARIANA_PLAYER_PROJECT}${pipeline}")
    list(APPEND player_files "${MARIANA_PLAYER_PROJECT}${pipeline}")
  endif()

  string(JSON spawn_count ERROR_VARIABLE json_error LENGTH "${player_config}" spawn)
  if(NOT json_error AND spawn_count GREATER 0)
    math(EXPR last_spawn "${spawn_count} - 1")
    foreach(i RANGE ${last_spawn})
      string(JSON prefab ERROR_VARIABLE json_error GET "${player_config}" spawn ${i} prefab)
      if(json_error)
        continue() # a primitive
      endif()
      mariana_player_add_file("${prefab}")
    endforeach()
  endif()
  list(REMOVE_DUPLICATES player_files)

  # A standalone site in player/ (index.html, .js, .wasm, .data), ready to upload as is. Kept out of
  # bin/, which is what the Pages workflow publishes (the editor).
  set_target_properties(MarianaPlayer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/player")
  mariana_web_app(MarianaPlayer
    OUTPUT_NAME index
    SHELL "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    PRELOAD ${player_files}
  )
endif()
