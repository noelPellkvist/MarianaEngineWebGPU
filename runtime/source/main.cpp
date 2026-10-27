#include <Player/PlayerApp.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>

int main() {
  // Resource paths are looked up in the project first, then the engine's resources.
  FileSystem::AddRoot(MARIANA_PROJECT_DIR);

  PlayerConfig config;
  std::string error;
  if (!PlayerConfig::Load("/Player.json", config, error))
    Logger::Error("Could not read the project's Player.json, running an empty scene: " + error);

  PlayerApp app(config);
  app.Start();
}
