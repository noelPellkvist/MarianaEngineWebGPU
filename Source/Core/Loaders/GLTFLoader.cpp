#include "GLTFLoader.hpp"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../External/tiny_gltf.h"

#include "../Logging.hpp"
#include "../Components/Relationship.hpp"
#include "../Components/Transform.hpp"


void LoadGLTFObject(Scene& scene)
{
    std::string fullpath = std::string(RESOURCE_DIR) + "/rumba.glb";
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool ret;
    
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, fullpath);

    if (!warn.empty()) {
      printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
      printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
      printf("Failed to parse glTF\n");
      return;
    }

    std::vector<entt::entity> entities;
    entities.reserve(model.nodes.size());
    for(auto& node : model.nodes)
    {
        entities.push_back(scene.CreateGameobject(node.name));
    }

    for(int i = 0; i < entities.size(); i++)
    {
        auto& currentNode = scene.GetEntities().get<Relationship>(entities[i]);
        for (int childIndex : model.nodes[i].children)
            currentNode.AddChild(entities[i], entities[childIndex], scene.GetEntities());
    }
    Logging::PrintSuccess("Succesfully loaded a gltf model");
}