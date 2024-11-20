#include "GameObject.hpp"
#include "Resources.h"

GameObject::GameObject() : name("Empty")
{
    //Resources::LoadOBJMesh(std::string("/") + name);
}

GameObject::GameObject(std::string name, std::string meshName) : name(name)
{
    Resources::LoadOBJMesh(std::string("/") + meshName);
}

GameObject::~GameObject()
{

}
