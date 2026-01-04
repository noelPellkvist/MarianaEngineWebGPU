cmake_minimum_required(VERSION 3.22)
project(app)
set(CMAKE_CXX_STANDARD 20)

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

file(GLOB_RECURSE APP_SOURCES
     CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

add_executable(app ${APP_SOURCES})

target_compile_definitions(app PRIVATE
  RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
  FLECS_STATIC
  FLECS_ENTITY_T=uint32_t
)

target_include_directories(app PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/Loaders"
)

target_link_libraries(app PRIVATE flecs::flecs_static)

# glm
add_subdirectory(external/glm)
target_link_libraries(app PRIVATE glm::glm)

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
#target_compile_definitions(imgui PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
  target_compile_definitions(app PRIVATE FLECS_NO_THREADS)

  target_link_libraries(imgui PUBLIC emdawnwebgpu_cpp)
  target_link_libraries(app PRIVATE emdawnwebgpu_cpp webgpu_glfw glm::glm imgui)
  set_target_properties(app PROPERTIES
    OUTPUT_NAME "index"
    SUFFIX ".html"
    LINK_DEPENDS "${CMAKE_SOURCE_DIR}/shell_modern.html"
  )

  # Compiler flags
  target_compile_options(app PRIVATE "SHELL:-fexceptions")

  # Linker flags
  target_link_options(app PRIVATE
    "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/shell_modern.html"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sINITIAL_MEMORY=256MB"
    "SHELL:-sSTACK_SIZE=262144"
    "-sASYNCIFY=1"
    "-sUSE_GLFW=3"
    "SHELL:-sFORCE_FILESYSTEM=1"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/resources"
  )

  # Copy branding next to index.html
  add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/logo.png"
            "$<TARGET_FILE_DIR:app>/logo.png"
    VERBATIM
  )
else()
  target_compile_definitions(imgui PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
  target_link_libraries(imgui PUBLIC webgpu_dawn webgpu_glfw glfw)
  target_link_libraries(app PRIVATE webgpu_dawn webgpu_glfw glfw imgui)
endif()
