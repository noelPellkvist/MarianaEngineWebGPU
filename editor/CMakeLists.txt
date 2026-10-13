# MarianaEditor: the editor app on MarianaEngine (ImGui UI, editor windows, picking, gizmo).

file(GLOB_RECURSE MARIANA_EDITOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_executable(MarianaEditor ${MARIANA_EDITOR_SOURCES})

target_include_directories(MarianaEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(MarianaEditor PRIVATE MarianaEngine imgui)

# The editor's own resources (font, icon), the project it opens (the sample one for now) and where
# it keeps its layout (imgui.ini).
set(MARIANA_PROJECT_DIR "${PROJECT_SOURCE_DIR}/projects/Sandbox")
target_compile_definitions(MarianaEditor PRIVATE
  MARIANA_EDITOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  MARIANA_EDITOR_RESOURCES="${CMAKE_CURRENT_SOURCE_DIR}/resources"
  MARIANA_PROJECT_DIR="${MARIANA_PROJECT_DIR}"
)

if(EMSCRIPTEN)
  # The editor's web build is the GitHub Pages site (index.html).
  mariana_web_app(MarianaEditor
    OUTPUT_NAME index
    SHELL "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    PRELOAD "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${MARIANA_PROJECT_DIR}"
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
