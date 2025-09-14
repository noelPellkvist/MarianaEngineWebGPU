cmake_minimum_required(VERSION 3.22)
project(app)
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE APP_SOURCES
     CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

add_executable(app ${APP_SOURCES})

target_compile_definitions(app PRIVATE
    RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

target_include_directories(app PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/Loaders"
)

# glm
add_subdirectory(external/glm)
target_link_libraries(app PRIVATE glm::glm)

# --- ImGui ---
add_library(imgui STATIC
    external/imgui/backends/imgui_impl_wgpu.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/misc/cpp/imgui_stdlib.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp         # ✅ include demo window
)

target_include_directories(imgui PUBLIC
    external/imgui
    external/imgui/backends
    external/imgui/misc/cpp
)

# --- Dawn ---
set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC)
add_subdirectory("dawn" EXCLUDE_FROM_ALL)

if(EMSCRIPTEN)
  # Output: index.html using our custom shell
  set_target_properties(app PROPERTIES
    OUTPUT_NAME "index"
    SUFFIX ".html"
    LINK_DEPENDS "${CMAKE_SOURCE_DIR}/shell_modern.html"
  )

  # Link Dawn + ImGui explicitly
  target_link_libraries(imgui PUBLIC emdawnwebgpu_cpp webgpu_glfw)
  target_link_libraries(app PRIVATE emdawnwebgpu_cpp webgpu_glfw)

  # Compiler flags
  target_compile_options(app PRIVATE "SHELL:-fexceptions")

  # Linker flags
  target_link_options(app PRIVATE
    "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/shell_modern.html"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sINITIAL_MEMORY=67108864"
    "SHELL:-sEXCEPTION_CATCHING_ALLOWED=['*']"
    "SHELL:-sASSERTIONS=1"
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
  # Link Dawn + GLFW explicitly
  target_compile_definitions(imgui PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
  target_link_libraries(imgui PUBLIC webgpu_dawn webgpu_glfw glfw)
  target_link_libraries(app PRIVATE webgpu_dawn webgpu_glfw glfw)
endif()

# Finally, app depends on imgui
target_link_libraries(app PRIVATE imgui)
