#pragma once
#include "GlobalVaribles.hpp"
#include <vector>
#include <string>

class Application;

class GameObject
{
    private:
    std::vector<GameObject> children;
    std::string name;
    Mesh mesh;

    public:
    GameObject();
    GameObject(std::string name, std::string meshName);
    ~GameObject();

    friend class Application;
};