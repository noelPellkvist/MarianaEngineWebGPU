#include <Mariana/Importers/AnimationGraphFile.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace
{
    using Json = nlohmann::ordered_json;

    // Floats written with 6 decimals at most, so 0.1f is saved as 0.1.
    double Clean(float value)
    {
        return std::round(static_cast<double>(value) * 1e6) / 1e6;
    }

    const char* const kTypeNames[] = { "clip", "blend", "blend1d", "blend2d", "additive", "layer", "stateMachine" };
    const char* const kParameterTypes[] = { "float", "bool", "trigger" };
    const char* const kConditionModes[] = { "if", "ifNot", "greater", "less", "equals", "notEquals" };

    template <size_t N>
    int IndexOf(const char* const (&names)[N], const std::string& name)
    {
        for (size_t i = 0; i < N; ++i)
            if (name == names[i])
                return static_cast<int>(i);
        return -1;
    }

    float Number(const Json& object, const char* key, float fallback)
    {
        const auto it = object.find(key);
        return it != object.end() && it->is_number() ? it->get<float>() : fallback;
    }

    bool Flag(const Json& object, const char* key, bool fallback)
    {
        const auto it = object.find(key);
        return it != object.end() && it->is_boolean() ? it->get<bool>() : fallback;
    }

    std::string Text(const Json& object, const char* key)
    {
        const auto it = object.find(key);
        return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
    }

    // Writes a graph: nodes by name where names are unique, by index otherwise.
    class Writer
    {
    public:
        explicit Writer(const AnimationGraph& graph) : m_Graph(graph)
        {
            for (const AnimationNode& node : graph.nodes)
                ++m_Names[node.name];
        }

        Json Node(int index) const
        {
            if (index < 0 || index >= static_cast<int>(m_Graph.nodes.size()))
                return nullptr;
            const std::string& name = m_Graph.nodes[index].name;
            return !name.empty() && m_Names.at(name) == 1 ? Json(name) : Json(index);
        }

        Json Parameter(int index) const
        {
            return index >= 0 && index < static_cast<int>(m_Graph.parameters.size()) ? Json(m_Graph.parameters[index].name) : Json(nullptr);
        }

        Json Weight(const AnimationWeight& weight) const
        {
            return weight.parameter >= 0 ? Parameter(weight.parameter) : Json(Clean(weight.value));
        }

    private:
        const AnimationGraph& m_Graph;
        std::unordered_map<std::string, int> m_Names;
    };

    Json Point(glm::vec2 p)
    {
        return Json::array({ Clean(p.x), Clean(p.y) });
    }

    glm::vec2 ReadPoint(const Json& object, const char* key, glm::vec2 fallback)
    {
        const auto it = object.find(key);
        if (it == object.end() || !it->is_array() || it->size() != 2 || !(*it)[0].is_number() || !(*it)[1].is_number())
            return fallback;
        return glm::vec2((*it)[0].get<float>(), (*it)[1].get<float>());
    }

    Json WriteNode(const AnimationNode& node, const Writer& writer)
    {
        Json json = Json::object();
        json["name"] = node.name;
        json["type"] = kTypeNames[static_cast<int>(node.type)];
        json["position"] = Point(node.position);
        auto inputs = [&]() {
            Json list = Json::array();
            for (int input : node.inputs)
                list.push_back(writer.Node(input));
            return list;
        };

        switch (node.type)
        {
        case AnimationNodeType::Clip:
            json["clip"] = node.clip;
            if (node.speed != 1.0f)
                json["speed"] = Clean(node.speed);
            if (!node.loop)
                json["loop"] = false;
            if (!node.syncGroup.empty())
                json["syncGroup"] = node.syncGroup;
            break;
        case AnimationNodeType::Blend:
            json["inputs"] = inputs();
            json["weight"] = writer.Weight(node.weight);
            break;
        case AnimationNodeType::Blend1D:
        {
            json["parameter"] = writer.Parameter(node.parameter);
            Json list = Json::array();
            for (size_t i = 0; i < node.inputs.size(); ++i)
                list.push_back({ { "node", writer.Node(node.inputs[i]) }, { "threshold", Clean(i < node.thresholds.size() ? node.thresholds[i] : 0.0f) } });
            json["inputs"] = list;
            break;
        }
        case AnimationNodeType::Blend2D:
        {
            json["parameters"] = Json::array({ writer.Parameter(node.parameter), writer.Parameter(node.parameterY) });
            Json list = Json::array();
            for (size_t i = 0; i < node.inputs.size(); ++i)
            {
                const glm::vec2 position = i < node.positions.size() ? node.positions[i] : glm::vec2(0.0f);
                list.push_back({ { "node", writer.Node(node.inputs[i]) }, { "position", Json::array({ Clean(position.x), Clean(position.y) }) } });
            }
            json["inputs"] = list;
            break;
        }
        case AnimationNodeType::Additive:
            json["inputs"] = inputs();
            json["weight"] = writer.Weight(node.weight);
            json["reference"] = node.reference == AdditiveReference::Rest ? "rest" : "firstFrame";
            break;
        case AnimationNodeType::Layer:
        {
            json["inputs"] = inputs();
            json["mode"] = node.layerMode == LayerMode::Additive ? "additive" : "override";
            if (node.layerMode == LayerMode::Additive)
                json["reference"] = node.reference == AdditiveReference::Rest ? "rest" : "firstFrame";
            Json mask = Json::array();
            for (const BoneMask::Entry& entry : node.mask.entries)
                mask.push_back({ { "bone", entry.bone }, { "weight", Clean(entry.weight) } });
            if (!mask.empty())
                json["mask"] = mask;
            json["weight"] = writer.Weight(node.weight);
            break;
        }
        case AnimationNodeType::StateMachine:
        {
            auto stateName = [&](int state) { return state >= 0 && state < static_cast<int>(node.states.size()) ? Json(node.states[state]) : Json(nullptr); };
            json["entry"] = stateName(node.entryState);
            json["anyStatePosition"] = Point(node.anyStatePosition);
            Json states = Json::array();
            for (size_t i = 0; i < node.states.size(); ++i)
                states.push_back({ { "name", node.states[i] }, { "node", writer.Node(i < node.inputs.size() ? node.inputs[i] : -1) },
                                   { "position", Point(i < node.statePositions.size() ? node.statePositions[i] : glm::vec2(0.0f)) } });
            json["states"] = states;

            Json transitions = Json::array();
            for (const AnimationTransition& transition : node.transitions)
            {
                Json t = Json::object();
                if (transition.from >= 0)
                    t["from"] = stateName(transition.from);
                t["to"] = stateName(transition.to);
                t["duration"] = Clean(transition.duration);
                if (transition.exitTime >= 0.0f)
                    t["exitTime"] = Clean(transition.exitTime);
                if (transition.interruptible)
                    t["interruptible"] = true;
                Json conditions = Json::array();
                for (const AnimationCondition& condition : transition.conditions)
                {
                    Json c = { { "parameter", writer.Parameter(condition.parameter) }, { "mode", kConditionModes[static_cast<int>(condition.mode)] } };
                    if (condition.mode != ConditionMode::If && condition.mode != ConditionMode::IfNot)
                        c["threshold"] = Clean(condition.threshold);
                    conditions.push_back(c);
                }
                if (!conditions.empty())
                    t["conditions"] = conditions;
                transitions.push_back(t);
            }
            json["transitions"] = transitions;
            break;
        }
        }
        return json;
    }

    // A node reference: a name or an index.
    int ReadNode(const AnimationGraph& graph, const Json& value)
    {
        if (value.is_number_integer())
            return value.get<int>();
        return value.is_string() ? graph.FindNode(value.get<std::string>()) : -1;
    }

    int ReadParameter(const AnimationGraph& graph, const Json& value)
    {
        return value.is_string() ? graph.FindParameter(value.get<std::string>()) : -1;
    }

    AnimationWeight ReadWeight(const AnimationGraph& graph, const Json& object)
    {
        const auto it = object.find("weight");
        if (it == object.end())
            return {};
        if (it->is_string())
            return { 0.0f, ReadParameter(graph, *it) };
        return { it->is_number() ? it->get<float>() : 1.0f, -1 };
    }

    void ReadNodeFields(AnimationGraph& graph, AnimationNode& node, const Json& json)
    {
        auto readInputs = [&]() {
            if (const auto it = json.find("inputs"); it != json.end() && it->is_array())
                for (const Json& input : *it)
                    node.inputs.push_back(ReadNode(graph, input));
        };
        auto reference = [&]() { return Text(json, "reference") == "rest" ? AdditiveReference::Rest : AdditiveReference::FirstFrame; };
        node.position = ReadPoint(json, "position", node.position);

        switch (node.type)
        {
        case AnimationNodeType::Clip:
            node.clip = Text(json, "clip");
            node.speed = Number(json, "speed", 1.0f);
            node.loop = Flag(json, "loop", true);
            node.syncGroup = Text(json, "syncGroup");
            break;
        case AnimationNodeType::Blend:
            readInputs();
            node.weight = ReadWeight(graph, json);
            break;
        case AnimationNodeType::Blend1D:
            node.parameter = json.contains("parameter") ? ReadParameter(graph, json["parameter"]) : -1;
            if (const auto it = json.find("inputs"); it != json.end() && it->is_array())
            {
                for (const Json& input : *it)
                {
                    node.inputs.push_back(input.contains("node") ? ReadNode(graph, input["node"]) : -1);
                    node.thresholds.push_back(Number(input, "threshold", 0.0f));
                }
            }
            break;
        case AnimationNodeType::Blend2D:
            if (const auto it = json.find("parameters"); it != json.end() && it->is_array() && it->size() == 2)
            {
                node.parameter = ReadParameter(graph, (*it)[0]);
                node.parameterY = ReadParameter(graph, (*it)[1]);
            }
            if (const auto it = json.find("inputs"); it != json.end() && it->is_array())
            {
                for (const Json& input : *it)
                {
                    node.inputs.push_back(input.contains("node") ? ReadNode(graph, input["node"]) : -1);
                    glm::vec2 position(0.0f);
                    if (const auto p = input.find("position"); p != input.end() && p->is_array() && p->size() == 2)
                        position = glm::vec2((*p)[0].get<float>(), (*p)[1].get<float>());
                    node.positions.push_back(position);
                }
            }
            break;
        case AnimationNodeType::Additive:
            readInputs();
            node.weight = ReadWeight(graph, json);
            node.reference = reference();
            break;
        case AnimationNodeType::Layer:
            readInputs();
            node.layerMode = Text(json, "mode") == "additive" ? LayerMode::Additive : LayerMode::Override;
            node.reference = reference();
            if (const auto it = json.find("mask"); it != json.end() && it->is_array())
                for (const Json& entry : *it)
                    node.mask.entries.push_back({ Text(entry, "bone"), Number(entry, "weight", 1.0f) });
            node.weight = ReadWeight(graph, json);
            break;
        case AnimationNodeType::StateMachine:
        {
            if (const auto it = json.find("states"); it != json.end() && it->is_array())
            {
                for (const Json& state : *it)
                {
                    node.states.push_back(Text(state, "name"));
                    node.inputs.push_back(state.contains("node") ? ReadNode(graph, state["node"]) : -1);
                    node.statePositions.push_back(ReadPoint(state, "position", glm::vec2(0.0f)));
                }
                node.anyStatePosition = ReadPoint(json, "anyStatePosition", node.anyStatePosition);
            }
            const std::string entry = Text(json, "entry");
            node.entryState = entry.empty() ? 0 : node.FindState(entry);
            if (const auto it = json.find("transitions"); it != json.end() && it->is_array())
            {
                for (const Json& t : *it)
                {
                    AnimationTransition transition;
                    const std::string from = Text(t, "from");
                    transition.from = from.empty() || from == "*" ? -1 : node.FindState(from);
                    if (!from.empty() && from != "*" && transition.from < 0)
                        transition.from = -2; // names no state: Validate reports it
                    transition.to = node.FindState(Text(t, "to"));
                    transition.duration = Number(t, "duration", transition.duration);
                    transition.exitTime = Number(t, "exitTime", -1.0f);
                    transition.interruptible = Flag(t, "interruptible", false);
                    if (const auto c = t.find("conditions"); c != t.end() && c->is_array())
                    {
                        for (const Json& condition : *c)
                        {
                            const int mode = IndexOf(kConditionModes, Text(condition, "mode"));
                            transition.conditions.push_back({ condition.contains("parameter") ? ReadParameter(graph, condition["parameter"]) : -1,
                                                              mode < 0 ? ConditionMode::If : static_cast<ConditionMode>(mode),
                                                              Number(condition, "threshold", 0.0f) });
                        }
                    }
                    node.transitions.push_back(std::move(transition));
                }
            }
            break;
        }
        }
    }

    std::filesystem::path ClipFile(const std::filesystem::path& graphFile, const std::string& file)
    {
        return !file.empty() && file[0] == '/' ? FileSystem::Resolve(file) : (graphFile.parent_path() / file).lexically_normal();
    }
}

namespace AnimationGraphFile
{
    std::shared_ptr<AnimationGraph> Load(AssetManager2& assets, const std::filesystem::path& file, std::string& error)
    {
        Json json;
        try
        {
            std::ifstream stream(file, std::ios::binary);
            if (!stream)
            {
                error = "Can't open " + file.string();
                return nullptr;
            }
            std::stringstream text;
            text << stream.rdbuf();
            json = Json::parse(text.str());
        }
        catch (const std::exception& e)
        {
            error = "Can't read " + file.string() + ": " + e.what();
            return nullptr;
        }

        auto graph = std::make_shared<AnimationGraph>();
        if (const auto it = json.find("parameters"); it != json.end() && it->is_array())
        {
            for (const Json& p : *it)
            {
                AnimationParameter parameter;
                parameter.name = Text(p, "name");
                const int type = IndexOf(kParameterTypes, Text(p, "type"));
                parameter.type = type < 0 ? AnimationParameterType::Float : static_cast<AnimationParameterType>(type);
                if (const auto d = p.find("default"); d != p.end())
                    parameter.defaultValue = d->is_boolean() ? (d->get<bool>() ? 1.0f : 0.0f) : d->is_number() ? d->get<float>() : 0.0f;
                graph->parameters.push_back(std::move(parameter));
            }
        }

        if (const auto it = json.find("clips"); it != json.end() && it->is_array())
        {
            for (const Json& c : *it)
            {
                const std::string name = Text(c, "name");
                const std::filesystem::path source = ClipFile(file, Text(c, "file"));
                const std::vector<NamedClip> clips = GLTF::GLTFLoader::LoadClips(assets, source);
                const std::string byName = Text(c, "clip");
                const size_t index = static_cast<size_t>(std::max(0.0f, Number(c, "animation", 0.0f)));
                const NamedClip* found = nullptr;
                for (size_t i = 0; i < clips.size() && !found; ++i)
                    if (byName.empty() ? i == index : clips[i].name == byName)
                        found = &clips[i];
                if (!found)
                {
                    Logger::Warning(file.string() + ": clip " + name + " isn't in " + source.string());
                    continue;
                }
                NamedClip clip{ name.empty() ? found->name : name, found->clip, {} };
                if (const auto events = c.find("events"); events != c.end() && events->is_array())
                    for (const Json& e : *events)
                        clip.events.push_back({ Number(e, "time", 0.0f), Text(e, "name") });
                graph->clips.push_back(std::move(clip));
            }
        }

        // Nodes first by name and type, so references to later nodes resolve.
        const Json* nodes = json.contains("nodes") && json["nodes"].is_array() ? &json["nodes"] : nullptr;
        if (nodes)
        {
            for (const Json& n : *nodes)
            {
                AnimationNode node;
                node.name = Text(n, "name");
                const int type = IndexOf(kTypeNames, Text(n, "type"));
                if (type < 0)
                {
                    error = file.string() + ": node " + node.name + " has an unknown type \"" + Text(n, "type") + "\"";
                    return nullptr;
                }
                node.type = static_cast<AnimationNodeType>(type);
                graph->nodes.push_back(std::move(node));
            }
            for (size_t i = 0; i < graph->nodes.size(); ++i)
                ReadNodeFields(*graph, graph->nodes[i], (*nodes)[i]);
        }

        graph->output = json.contains("output") ? ReadNode(*graph, json["output"]) : static_cast<int>(graph->nodes.size()) - 1;
        std::string problem;
        if (!graph->Validate(problem))
            Logger::Warning(file.string() + " isn't usable yet: " + problem);
        return graph;
    }

    bool Save(const AnimationGraph& graph, AssetManager2& assets, const std::filesystem::path& file, std::string& error)
    {
        const Writer writer(graph);
        Json json = Json::object();

        Json parameters = Json::array();
        for (const AnimationParameter& parameter : graph.parameters)
        {
            Json p = { { "name", parameter.name }, { "type", kParameterTypes[static_cast<int>(parameter.type)] } };
            if (parameter.type == AnimationParameterType::Float)
                p["default"] = Clean(parameter.defaultValue);
            else if (parameter.type == AnimationParameterType::Bool)
                p["default"] = parameter.defaultValue != 0.0f;
            parameters.push_back(p);
        }
        json["parameters"] = parameters;

        Json clips = Json::array();
        for (const NamedClip& clip : graph.clips)
        {
            std::filesystem::path source;
            size_t index = 0;
            if (!GLTF::GLTFLoader::ClipSource(assets, clip.clip.GetID(), source, index))
            {
                error += std::string(error.empty() ? "" : "\n") + "Clip " + clip.name + " doesn't come from a glTF file and was left out";
                continue;
            }
            std::error_code ec;
            std::filesystem::path relative = std::filesystem::relative(source, file.parent_path(), ec);
            if (ec || relative.empty())
                relative = source;
            Json entry = { { "name", clip.name }, { "file", relative.generic_string() }, { "animation", index } };
            if (!clip.events.empty())
            {
                Json events = Json::array();
                for (const AnimationEvent& event : clip.events)
                    events.push_back({ { "time", event.time }, { "name", event.name } });
                entry["events"] = events;
            }
            clips.push_back(entry);
        }
        if (!clips.empty())
            json["clips"] = clips;

        Json nodes = Json::array();
        for (const AnimationNode& node : graph.nodes)
            nodes.push_back(WriteNode(node, writer));
        json["nodes"] = nodes;
        json["output"] = writer.Node(graph.output);

        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Can't write " + file.string();
            return false;
        }
        stream << json.dump(2) << "\n";
        return true;
    }

    void RegisterLoader(AssetManager2& assets)
    {
        assets.RegisterLoader<AnimationGraph>(AssetType::AnimationGraph, { ".animgraph" },
            [](AssetManager2& manager, const AssetMetadata& meta) {
                std::string error;
                std::shared_ptr<AnimationGraph> graph = Load(manager, meta.path, error);
                if (!graph)
                    Logger::Error(error);
                return graph;
            });
    }
}
