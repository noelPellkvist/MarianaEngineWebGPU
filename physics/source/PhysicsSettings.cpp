#include <Mariana/Physics/PhysicsSettings.hpp>

#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
    using Json = nlohmann::ordered_json;

    double Clean(float value)
    {
        return std::round(static_cast<double>(value) * 1e6) / 1e6;
    }
}

bool PhysicsSettings::Collides(int a, int b) const
{
    if (a < 0 || b < 0 || a >= static_cast<int>(collisions.size()) || b >= kMaxLayers)
        return true;
    return (collisions[a] >> b) & 1u;
}

void PhysicsSettings::SetCollides(int a, int b, bool collides)
{
    if (a < 0 || b < 0 || a >= kMaxLayers || b >= kMaxLayers)
        return;
    if (collisions.size() <= static_cast<size_t>(std::max(a, b)))
        collisions.resize(static_cast<size_t>(std::max(a, b)) + 1, ~0u);
    const uint32_t bitA = 1u << a, bitB = 1u << b;
    collisions[a] = collides ? collisions[a] | bitB : collisions[a] & ~bitB;
    collisions[b] = collides ? collisions[b] | bitA : collisions[b] & ~bitA;
}

int PhysicsSettings::AddLayer(std::string name)
{
    if (static_cast<int>(layers.size()) >= kMaxLayers)
        return -1;
    layers.push_back(std::move(name));
    collisions.resize(layers.size(), ~0u);
    const int index = static_cast<int>(layers.size()) - 1;
    for (int other = 0; other < index; ++other)
        SetCollides(index, other, true);
    return index;
}

bool PhysicsSettings::Load(const std::filesystem::path& file, std::string& error)
{
    Json json;
    try
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
        {
            error = "Can't open " + file.string();
            return false;
        }
        std::stringstream text;
        text << stream.rdbuf();
        json = Json::parse(text.str());
    }
    catch (const std::exception& e)
    {
        error = "Can't read " + file.string() + ": " + e.what();
        return false;
    }

    *this = PhysicsSettings();
    if (const auto it = json.find("gravity"); it != json.end() && it->is_array() && it->size() == 3)
        gravity = glm::vec3((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>());
    fixedTimestep = std::max(json.value("fixedTimestep", fixedTimestep), 1e-4f);
    maxStepsPerFrame = std::max(json.value("maxStepsPerFrame", maxStepsPerFrame), 1);
    collisionSteps = std::max(json.value("collisionSteps", collisionSteps), 1);
    interpolation = json.value("interpolation", interpolation);
    penetrationSlop = std::max(json.value("penetrationSlop", penetrationSlop), 0.0f);
    speculativeContactDistance = std::max(json.value("speculativeContactDistance", speculativeContactDistance), 0.0f);

    if (const auto it = json.find("layers"); it != json.end() && it->is_array() && !it->empty())
    {
        layers.clear();
        for (const Json& layer : *it)
            if (static_cast<int>(layers.size()) < kMaxLayers)
                layers.push_back(layer.value("name", std::string("Layer ") + std::to_string(layers.size())));
        collisions.assign(layers.size(), 0u);
        for (size_t i = 0; i < layers.size(); ++i)
        {
            const Json& layer = (*it)[i];
            const auto with = layer.find("collidesWith");
            if (with == layer.end() || !with->is_array())
            {
                collisions[i] = ~0u;
                continue;
            }
            for (const Json& name : *with)
                for (size_t j = 0; j < layers.size(); ++j)
                    if (name.is_string() && name.get<std::string>() == layers[j])
                        collisions[i] |= 1u << j;
        }
        // Keep it symmetric: a pair collides only when both say so.
        for (size_t i = 0; i < layers.size(); ++i)
            for (size_t j = 0; j < layers.size(); ++j)
                if (!Collides(static_cast<int>(i), static_cast<int>(j)) || !Collides(static_cast<int>(j), static_cast<int>(i)))
                    SetCollides(static_cast<int>(i), static_cast<int>(j), false);
    }
    return true;
}

bool PhysicsSettings::Save(const std::filesystem::path& file, std::string& error) const
{
    Json json = Json::object();
    json["gravity"] = Json::array({ Clean(gravity.x), Clean(gravity.y), Clean(gravity.z) });
    json["fixedTimestep"] = Clean(fixedTimestep);
    json["maxStepsPerFrame"] = maxStepsPerFrame;
    json["collisionSteps"] = collisionSteps;
    json["interpolation"] = interpolation;
    json["penetrationSlop"] = Clean(penetrationSlop);
    json["speculativeContactDistance"] = Clean(speculativeContactDistance);
    Json list = Json::array();
    for (size_t i = 0; i < layers.size(); ++i)
    {
        Json with = Json::array();
        for (size_t j = 0; j < layers.size(); ++j)
            if (Collides(static_cast<int>(i), static_cast<int>(j)))
                with.push_back(layers[j]);
        list.push_back({ { "name", layers[i] }, { "collidesWith", with } });
    }
    json["layers"] = list;

    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "Can't write " + file.string();
        return false;
    }
    stream << json.dump(2) << "\n";
    return true;
}
