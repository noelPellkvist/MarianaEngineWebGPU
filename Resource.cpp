#include "Resource.h"
#include <iostream>
#include <fstream>
#include <sstream>

std::string MarianaEngine::Resource::LoadShaderCode(const std::string& name)
{
    std::ifstream file(name);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << name << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
