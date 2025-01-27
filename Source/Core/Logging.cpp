#include "Logging.hpp"
#include <iostream>

void Logging::PrintInfo(std::string msg)
{
    std::cout << "[INFO]: " << msg << "\n";
}

void Logging::PrintSuccess(std::string msg)
{
    std::cout << "\033[1;32m[SUCCESS]: " << msg << "\033[0m\n";
}

void Logging::PrintWarning(std::string msg)
{
    std::cout << "\033[1;33m[WARNING]: " << msg << "\033[0m\n";
}

void Logging::PrintError(std::string msg)
{
    std::cout << "\033[1;31m[Error]: " << msg << "\033[0m\n";
}
