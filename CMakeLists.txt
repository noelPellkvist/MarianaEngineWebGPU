cmake_minimum_required(VERSION 3.22)
project(MarianaEngine)
set(CMAKE_CXX_STANDARD 20)

# ---- Third-party dependencies --------------------------------------------------------------

include(FetchContent)

set(FLECS_STATIC ON  CACHE BOOL "" FORCE)
set(FLECS_SHARED OFF CACHE BOOL "" FORCE)
set(FLECS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLECS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  flecs
  GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
  GIT_TAG v4.1.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(flecs)

get_target_property(FLECS_REAL_TARGET flecs::flecs_static ALIASED_TARGET)

if(FLECS_REAL_TARGET)
  target_compile_definitions(${FLECS_REAL_TARGET} PUBLIC
    FLECS_STATIC
    FLECS_ENTITY_T=uint32_t
  )
endif()

# glm
add_subdirectory(external/glm)

# ---- Dawn configuration (MUST be before add_subdirectory) ----
set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC CACHE STRING "" FORCE)

set(DAWN_ENABLE_GEN ON CACHE BOOL "" FORCE)

# Emscripten + WebGPU
set(DAWN_ENABLE_EMSCRIPTEN ON CACHE BOOL "" FORCE)
set(DAWN_ENABLE_EMDAWNWEBGPU ON CACHE BOOL "" FORCE)

set(DAWN_ENABLE_WEBGPU_C ON CACHE BOOL "" FORCE)
set(DAWN_ENABLE_WEBGPU_CPP ON CACHE BOOL "" FORCE)
set(DAWN_ENABLE_WGSL ON CACHE BOOL "" FORCE)

set(DAWN_DISABLE_PROGRAM_CACHE ON CACHE BOOL "" FORCE)
# -------------------------------------------------------------

add_subdirectory(dawn EXCLUDE_FROM_ALL)

add_subdirectory(external/imgui)

if(EMSCRIPTEN)
  if(FLECS_REAL_TARGET)
    target_compile_definitions(${FLECS_REAL_TARGET} PUBLIC FLECS_NO_THREADS)
  endif()
  target_link_libraries(imgui PUBLIC emdawnwebgpu_cpp)
else()
  target_compile_definitions(imgui PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
  target_link_libraries(imgui PUBLIC webgpu_dawn webgpu_glfw glfw)
endif()

# ---- Mariana -------------------------------------------------------------------------------

# Every app lands in bin/ (bin/<Config>/ with multi-config generators like Visual Studio).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# The engine's built-in resources (shaders, effects, pipelines, the default sky); every app needs them.
set(MARIANA_ENGINE_RESOURCES "${PROJECT_SOURCE_DIR}/engine/resources")

include(cmake/MarianaWeb.cmake)

# The engine library, then the apps built on it. The engine never depends on an app.
add_subdirectory(engine)
add_subdirectory(editor)
add_subdirectory(runtime)
