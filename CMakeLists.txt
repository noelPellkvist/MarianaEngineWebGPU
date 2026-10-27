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

# Jolt Physics (used by MarianaPhysics only). Built as a plain static library: no samples, tests,
# warnings-as-errors, LTO or GPU compute backends, with the shared MSVC runtime and RTTI like the
# rest of the project. WASM SIMD on the web (every current browser has it).
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
  set(USE_WASM_SIMD ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
  JoltPhysics
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG v5.6.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(JoltPhysics)

# Luau (used by MarianaScripting only): the VM and the compiler, without its command line tools and
# tests. Native code generation natively (the web runs the interpreter).
set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  Luau
  GIT_REPOSITORY https://github.com/luau-lang/luau.git
  GIT_TAG 0.744
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Luau)
# Only what MarianaScripting links gets built (not the type checker and the rest).
set_property(DIRECTORY "${luau_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL ON)

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
# The scripts MarianaScripting comes with (apps that link it add the folder as a root).
set(MARIANA_SCRIPTING_RESOURCES "${PROJECT_SOURCE_DIR}/scripting/resources")

include(cmake/MarianaWeb.cmake)

# The engine library, the physics and scripting libraries on it, then the apps built on them. The
# engine never depends on an app (or on physics or scripting).
add_subdirectory(engine)
add_subdirectory(physics)
add_subdirectory(scripting)
add_subdirectory(editor)
add_subdirectory(runtime)
