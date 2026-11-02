#pragma once
// What scripts keep between runs of the game (the save library): a JSON object read the first time
// a script uses it and written when it changed. Private to MarianaScripting.
#include <json.hpp>

#include <string>

namespace ScriptingInternal
{
    struct SaveData
    {
        std::string location; // a file natively, a localStorage key on the web; empty = not kept
        nlohmann::json data = nlohmann::json::object();
        bool loaded = false;
        bool dirty = false;

        // Reads it the first time (a missing or broken store starts empty).
        void Load();
        // Writes it when it changed.
        void Flush();
    };
}
