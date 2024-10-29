#pragma once
#include <string>

namespace MarianaEngine
{
	class Resource
	{
	public:
		static std::string LoadShaderCode(const std::string& filepath);
	};
}