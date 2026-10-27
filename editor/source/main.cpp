#include <Editor/EditorApp.hpp>
#include <Mariana/Foundation/FileSystem.hpp>

int main() {
  // Resource paths are looked up in the project first, then the editor's resources, then the engine's.
  FileSystem::AddRoot(MARIANA_EDITOR_RESOURCES);
  FileSystem::AddRoot(MARIANA_PROJECT_DIR);

  EditorApp app;
  app.Start();
}
