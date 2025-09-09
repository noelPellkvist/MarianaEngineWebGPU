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

target_include_directories(app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(external/glm)
target_link_libraries(app PRIVATE glm::glm)

# Dawn
set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC)
add_subdirectory("dawn" EXCLUDE_FROM_ALL)

if(EMSCRIPTEN)
  # --- Output: index.html using our custom shell ---
  set_target_properties(app PROPERTIES
    OUTPUT_NAME "index"
    SUFFIX ".html"
    LINK_DEPENDS "${CMAKE_SOURCE_DIR}/shell_modern.html"  # rebuild when shell changes
  )

  # Dawn + GLFW (from your existing subdirs)
  target_link_libraries(app PRIVATE emdawnwebgpu_cpp webgpu_glfw)

  # Compiler flags (keep linker-only -s flags out of here)
  target_compile_options(app PRIVATE
    "SHELL:-fexceptions"
  )

  # Linker flags
  target_link_options(app PRIVATE
    "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/shell_modern.html"

    # Memory & runtime
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sINITIAL_MEMORY=67108864"
    "SHELL:-sEXCEPTION_CATCHING_ALLOWED=['*']"
    "SHELL:-sASSERTIONS=1"
    "-sASYNCIFY=1"

    # GLFW JS glue
    "-sUSE_GLFW=3"

    # Virtual FS + resources
    "SHELL:-sFORCE_FILESYSTEM=1"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/resources"

    # IMPORTANT: Do NOT use -sUSE_WEBGPU when using Dawn's JS libraries.
    # (They’re already provided via emdawnwebgpu and will conflict with -sUSE_WEBGPU)
  )

  # Copy branding next to index.html
  add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/logo.png"
            "$<TARGET_FILE_DIR:app>/logo.png"
    VERBATIM
  )
else()
  target_link_libraries(app PRIVATE webgpu_dawn webgpu_glfw glfw)
endif()

