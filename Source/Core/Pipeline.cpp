#include "Pipeline.hpp"
#include "Resources.h"

Pipeline::Pipeline(const char* shaderName)
{
    shaderModule = Resources::LoadShader(shaderName);
}

Pipeline::~Pipeline()
{
    
}