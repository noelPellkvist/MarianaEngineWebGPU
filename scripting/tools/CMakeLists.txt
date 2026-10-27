# MarianaScriptTools: what editing scripts needs and running them doesn't: the API's types for
# Luau's type checker (also written out for VS Code's luau-lsp), checking scripts and completing code.
# On Luau.Analysis, which only the editor (and the scripting test) link; the player stays small.

file(GLOB_RECURSE MARIANA_SCRIPT_TOOLS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_library(MarianaScriptTools STATIC ${MARIANA_SCRIPT_TOOLS_SOURCES})

target_include_directories(MarianaScriptTools
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

target_link_libraries(MarianaScriptTools
  PUBLIC  MarianaScripting
  PRIVATE Luau.Analysis
)

# Where the hand-written part of the API's types is (resources/Core.d.luau).
target_compile_definitions(MarianaScriptTools PRIVATE MARIANA_SCRIPT_TOOLS_RESOURCES="${MARIANA_SCRIPT_TOOLS_RESOURCES}")

if(EMSCRIPTEN)
  # The checker reports what it can't go on with as C++ exceptions too.
  foreach(luau_target Luau.Analysis Luau.Config)
    target_compile_options(${luau_target} PRIVATE "SHELL:-fexceptions")
  endforeach()
endif()
