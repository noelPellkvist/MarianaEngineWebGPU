# MarianaEngine: everything a game needs at runtime (platform, GPU layer, ECS, assets, rendering,
# animation, importers), as a static library. Apps (the editor, the player) link it; it never
# includes anything of theirs. Public headers are included as <Mariana/...>.

file(GLOB_RECURSE MARIANA_ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_library(MarianaEngine STATIC ${MARIANA_ENGINE_SOURCES})

target_include_directories(MarianaEngine
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source"
          "${PROJECT_SOURCE_DIR}/external/Loaders"
          "${PROJECT_SOURCE_DIR}/external/tinygltf"
)

# Until the resources are split per project, every app reads the repository's resources folder.
target_compile_definitions(MarianaEngine PUBLIC RESOURCE_DIR="${PROJECT_SOURCE_DIR}/resources")

# The ECS wrapper is opaque, so flecs stays private to the engine.
target_link_libraries(MarianaEngine
  PUBLIC  glm::glm
  PRIVATE flecs::flecs_static
)

if(EMSCRIPTEN)
  target_compile_definitions(MarianaEngine PRIVATE FLECS_NO_THREADS)
  # Apps must compile with the same exception model as the engine.
  target_compile_options(MarianaEngine PUBLIC "SHELL:-fexceptions")
  target_link_libraries(MarianaEngine PUBLIC emdawnwebgpu_cpp webgpu_glfw)
else()
  target_link_libraries(MarianaEngine PUBLIC webgpu_dawn webgpu_glfw glfw)
endif()
