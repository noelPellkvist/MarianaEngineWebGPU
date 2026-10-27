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

# The default engine root of FileSystem (apps add their own roots in front of it).
target_compile_definitions(MarianaEngine PRIVATE MARIANA_ENGINE_RESOURCES="${MARIANA_ENGINE_RESOURCES}")

# The ECS wrapper is opaque, so flecs stays private to the engine.
target_link_libraries(MarianaEngine
  PUBLIC  glm::glm
  PRIVATE flecs::flecs_static
)

# MarianaEngineTest: a console program that runs tests/*Scenarios.hpp (the profiler, animation) and
# prints what they measured. No window, no GPU.
if(NOT EMSCRIPTEN)
  add_executable(MarianaEngineTest "${CMAKE_CURRENT_SOURCE_DIR}/tests/EngineTest.cpp")
  target_link_libraries(MarianaEngineTest PRIVATE MarianaEngine)
  target_include_directories(MarianaEngineTest PRIVATE "${PROJECT_SOURCE_DIR}/external/Loaders")
endif()

if(EMSCRIPTEN)
  target_compile_definitions(MarianaEngine PRIVATE FLECS_NO_THREADS)
  # Apps must compile with the same exception model as the engine, and link with it too (without it
  # at link time Emscripten turns catching off, and the first exception caught terminates).
  target_compile_options(MarianaEngine PUBLIC "SHELL:-fexceptions")
  target_link_options(MarianaEngine PUBLIC "SHELL:-fexceptions")
  target_link_libraries(MarianaEngine PUBLIC emdawnwebgpu_cpp webgpu_glfw)
else()
  target_link_libraries(MarianaEngine PUBLIC webgpu_dawn webgpu_glfw glfw)
endif()
