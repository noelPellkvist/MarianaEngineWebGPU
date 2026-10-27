# MarianaScripting: game code in Luau for apps built on MarianaEngine. A static library like the
# engine and physics; apps that want scripts link it. Scripts reach the scene's entities and their
# components, input, the game clock and physics. Luau stays private: the public headers use engine
# types only.

file(GLOB_RECURSE MARIANA_SCRIPTING_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_library(MarianaScripting STATIC ${MARIANA_SCRIPTING_SOURCES})

target_include_directories(MarianaScripting
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

target_link_libraries(MarianaScripting
  PUBLIC  MarianaEngine MarianaPhysics
  PRIVATE Luau.VM Luau.Compiler
)

if(EMSCRIPTEN)
  # Luau raises its errors as C++ exceptions, so it must catch them the way the engine does.
  foreach(luau_target Luau.Ast Luau.Bytecode Luau.Compiler Luau.VM)
    target_compile_options(${luau_target} PRIVATE "SHELL:-fexceptions")
  endforeach()
else()
  # Natively, script functions are compiled to machine code (Luau's code generator).
  target_link_libraries(MarianaScripting PRIVATE Luau.CodeGen)
  target_compile_definitions(MarianaScripting PRIVATE MARIANA_LUAU_CODEGEN=1)
endif()

# Checking and completing scripts (the editor's): a library of its own on Luau's type checker.
add_subdirectory(tools)

# MarianaScriptingTest: a console program that runs scripts against a scene made in code and checks
# what they did (the Sandbox's control scripts among them), and the scripts' types. No window, no
# GPU.
if(NOT EMSCRIPTEN)
  add_executable(MarianaScriptingTest "${CMAKE_CURRENT_SOURCE_DIR}/tests/ScriptTest.cpp")
  target_link_libraries(MarianaScriptingTest PRIVATE MarianaScriptTools)
  target_compile_definitions(MarianaScriptingTest PRIVATE MARIANA_SANDBOX_DIR="${PROJECT_SOURCE_DIR}/projects/Sandbox")
endif()
