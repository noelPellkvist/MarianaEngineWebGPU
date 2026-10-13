# MarianaEditor: the editor app on MarianaEngine (ImGui UI, editor windows, picking, gizmo).

file(GLOB_RECURSE MARIANA_EDITOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_executable(MarianaEditor ${MARIANA_EDITOR_SOURCES})

target_include_directories(MarianaEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(MarianaEditor PRIVATE MarianaEngine imgui)

if(EMSCRIPTEN)
  # The editor's web build is the GitHub Pages site (index.html).
  mariana_web_app(MarianaEditor
    OUTPUT_NAME index
    SHELL "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    PRELOAD "${PROJECT_SOURCE_DIR}/resources"
  )
  # Branding next to index.html
  add_custom_command(TARGET MarianaEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/logo.png"
            "$<TARGET_FILE_DIR:MarianaEditor>/logo.png"
    VERBATIM
  )
elseif(WIN32)
  target_sources(MarianaEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()
