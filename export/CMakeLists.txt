# MarianaExport: build profiles and the exporter that makes a game of a project (the player built
# for Windows or the web, with the files the game uses). The editor's File > Build and the
# MarianaExport command line tool use it. Building runs CMake and Emscripten's tools, so it works
# natively only; on the web the editor edits profiles.

file(GLOB_RECURSE MARIANA_EXPORT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_library(MarianaExport STATIC ${MARIANA_EXPORT_SOURCES})
target_include_directories(MarianaExport
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/external/Loaders"
)
target_link_libraries(MarianaExport PUBLIC MarianaEngine)
# What the exporter builds with by default: this source tree, its CMake and this build folder.
target_compile_definitions(MarianaExport PRIVATE
  MARIANA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  MARIANA_CMAKE_COMMAND="${CMAKE_COMMAND}"
  MARIANA_NATIVE_BUILD_DIR="${CMAKE_BINARY_DIR}"
  MARIANA_ENGINE_RESOURCES="${MARIANA_ENGINE_RESOURCES}"
)

if(NOT EMSCRIPTEN)
  # MarianaExport.exe --project <folder> [--profile <name>] [--list]: builds a profile from the
  # command line (CI, scripts).
  add_executable(MarianaExportTool "${CMAKE_CURRENT_SOURCE_DIR}/tools/ExportMain.cpp")
  set_target_properties(MarianaExportTool PROPERTIES OUTPUT_NAME MarianaExport)
  target_link_libraries(MarianaExportTool PRIVATE MarianaExport)

  # MarianaExportTest: what the Sandbox's profiles package and the Player.json they write. A console
  # program; no window, no GPU, nothing built.
  add_executable(MarianaExportTest "${CMAKE_CURRENT_SOURCE_DIR}/tests/ExportTest.cpp")
  target_link_libraries(MarianaExportTest PRIVATE MarianaExport)
  target_include_directories(MarianaExportTest PRIVATE "${PROJECT_SOURCE_DIR}/external/Loaders")
  target_compile_definitions(MarianaExportTest PRIVATE MARIANA_SANDBOX_DIR="${PROJECT_SOURCE_DIR}/projects/Sandbox")
endif()
