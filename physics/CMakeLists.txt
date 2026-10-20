# MarianaPhysics: rigid bodies and colliders (later joints, characters and vehicles) on Jolt Physics,
# for apps built on MarianaEngine. A static library like the engine; apps that want physics link it.
# Jolt stays private: the public headers use glm and engine types only.

file(GLOB_RECURSE MARIANA_PHYSICS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_library(MarianaPhysics STATIC ${MARIANA_PHYSICS_SOURCES})

target_include_directories(MarianaPhysics
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source"
          "${PROJECT_SOURCE_DIR}/external/Loaders"
)

target_link_libraries(MarianaPhysics
  PUBLIC  MarianaEngine
  PRIVATE Jolt
)
