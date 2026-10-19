#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Animation/Skeleton.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <functional>

std::vector<float> BoneMask::Resolve(const Skeleton& skeleton) const
{
    const size_t count = skeleton.BoneCount();
    if (entries.empty())
        return std::vector<float>(count, 1.0f);

    // Bones come parents first, so a bone without an entry takes its parent's weight.
    std::vector<float> own(count, -1.0f);
    for (const Entry& entry : entries)
        if (const int bone = skeleton.FindBone(entry.bone); bone >= 0)
            own[bone] = entry.weight;

    std::vector<float> weights(count, 0.0f);
    for (size_t i = 0; i < count; ++i)
    {
        const int parent = skeleton.GetBone(i).parent;
        weights[i] = own[i] >= 0.0f ? own[i] : parent >= 0 ? weights[parent] : 0.0f;
    }
    return weights;
}

int AnimationGraph::FindParameter(std::string_view name) const
{
    for (size_t i = 0; i < parameters.size(); ++i)
        if (parameters[i].name == name)
            return static_cast<int>(i);
    return -1;
}

int AnimationGraph::FindNode(std::string_view name) const
{
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].name == name)
            return static_cast<int>(i);
    return -1;
}

bool AnimationGraph::Validate(std::string& error) const
{
    const int count = static_cast<int>(nodes.size());
    auto describe = [&](int node) { return "node " + std::to_string(node) + " (" + nodes[node].name + ")"; };
    auto floatParameter = [&](int parameter) {
        return parameter >= 0 && parameter < static_cast<int>(parameters.size()) && parameters[parameter].type == AnimationParameterType::Float;
    };

    if (output < 0 || output >= count)
    {
        error = "no output node";
        return false;
    }

    for (int i = 0; i < count; ++i)
    {
        const AnimationNode& node = nodes[i];
        for (int input : node.inputs)
        {
            if (input < 0 || input >= count)
            {
                error = describe(i) + " has an input that isn't a node";
                return false;
            }
        }

        size_t inputs = 2;
        switch (node.type)
        {
        case AnimationNodeType::Clip:
            inputs = 0;
            if (node.clip.empty())
            {
                error = describe(i) + " names no clip";
                return false;
            }
            break;
        case AnimationNodeType::Blend1D:
            inputs = node.inputs.size();
            if (inputs == 0 || node.thresholds.size() != inputs)
            {
                error = describe(i) + " needs a threshold per input";
                return false;
            }
            for (size_t t = 1; t < node.thresholds.size(); ++t)
            {
                if (node.thresholds[t] < node.thresholds[t - 1])
                {
                    error = describe(i) + " has thresholds out of order";
                    return false;
                }
            }
            if (!floatParameter(node.parameter))
            {
                error = describe(i) + " needs a float parameter";
                return false;
            }
            break;
        default:
            if (node.weight.parameter >= 0 && !floatParameter(node.weight.parameter))
            {
                error = describe(i) + " has a weight parameter that isn't a float";
                return false;
            }
            break;
        }
        if (node.inputs.size() != inputs)
        {
            error = describe(i) + " needs " + std::to_string(inputs) + " inputs";
            return false;
        }
    }

    // Inputs must not lead back to the node (a node may feed several others).
    std::vector<uint8_t> state(count, 0); // 0 unvisited, 1 on the path, 2 done
    std::function<bool(int)> visit = [&](int node) {
        if (state[node] == 1)
            return false;
        if (state[node] == 2)
            return true;
        state[node] = 1;
        for (int input : nodes[node].inputs)
            if (!visit(input))
                return false;
        state[node] = 2;
        return true;
    };
    for (int i = 0; i < count; ++i)
    {
        if (!visit(i))
        {
            error = "the inputs of " + describe(i) + " form a cycle";
            return false;
        }
    }
    return true;
}

int AnimationGraph::Builder::Float(std::string name, float value)
{
    m_Graph.parameters.push_back({ std::move(name), AnimationParameterType::Float, value });
    return static_cast<int>(m_Graph.parameters.size()) - 1;
}

int AnimationGraph::Builder::Bool(std::string name, bool value)
{
    m_Graph.parameters.push_back({ std::move(name), AnimationParameterType::Bool, value ? 1.0f : 0.0f });
    return static_cast<int>(m_Graph.parameters.size()) - 1;
}

int AnimationGraph::Builder::Trigger(std::string name)
{
    m_Graph.parameters.push_back({ std::move(name), AnimationParameterType::Trigger, 0.0f });
    return static_cast<int>(m_Graph.parameters.size()) - 1;
}

AnimationWeight AnimationGraph::Builder::Parameter(std::string_view name) const
{
    const int parameter = m_Graph.FindParameter(name);
    if (parameter < 0)
        Logger::Warning("Animation graph: no parameter " + std::string(name));
    return { 0.0f, parameter };
}

int AnimationGraph::Builder::Add(AnimationNode node)
{
    m_Graph.nodes.push_back(std::move(node));
    m_Graph.output = static_cast<int>(m_Graph.nodes.size()) - 1;
    return m_Graph.output;
}

int AnimationGraph::Builder::Clip(std::string clip)
{
    return Clip(std::move(clip), ClipOptions{});
}

int AnimationGraph::Builder::Clip(std::string clip, ClipOptions options)
{
    AnimationNode node;
    node.type = AnimationNodeType::Clip;
    node.name = clip;
    node.clip = std::move(clip);
    node.speed = options.speed;
    node.loop = options.loop;
    node.syncGroup = std::move(options.syncGroup);
    return Add(std::move(node));
}

int AnimationGraph::Builder::Blend(int a, int b, AnimationWeight weight)
{
    AnimationNode node;
    node.type = AnimationNodeType::Blend;
    node.name = "Blend";
    node.inputs = { a, b };
    node.weight = weight;
    return Add(std::move(node));
}

int AnimationGraph::Builder::Blend1D(std::string_view parameter, std::vector<std::pair<int, float>> inputs)
{
    AnimationNode node;
    node.type = AnimationNodeType::Blend1D;
    node.name = "Blend1D";
    node.parameter = Parameter(parameter).parameter;
    for (const auto& [input, threshold] : inputs)
    {
        node.inputs.push_back(input);
        node.thresholds.push_back(threshold);
    }
    return Add(std::move(node));
}

int AnimationGraph::Builder::Additive(int base, int additive, AnimationWeight weight, AdditiveReference reference)
{
    AnimationNode node;
    node.type = AnimationNodeType::Additive;
    node.name = "Additive";
    node.inputs = { base, additive };
    node.weight = weight;
    node.reference = reference;
    return Add(std::move(node));
}

int AnimationGraph::Builder::Layer(int base, int layer, BoneMask mask, AnimationWeight weight, LayerMode mode)
{
    AnimationNode node;
    node.type = AnimationNodeType::Layer;
    node.name = "Layer";
    node.inputs = { base, layer };
    node.mask = std::move(mask);
    node.weight = weight;
    node.layerMode = mode;
    return Add(std::move(node));
}

AnimationGraph::Builder& AnimationGraph::Builder::Name(int node, std::string name)
{
    if (node >= 0 && node < static_cast<int>(m_Graph.nodes.size()))
        m_Graph.nodes[node].name = std::move(name);
    return *this;
}

AnimationGraph::Builder& AnimationGraph::Builder::Output(int node)
{
    m_Graph.output = node;
    return *this;
}

std::shared_ptr<AnimationGraph> AnimationGraph::Builder::Build()
{
    std::string error;
    if (!m_Graph.Validate(error))
    {
        Logger::Error("Animation graph: " + error);
        return nullptr;
    }
    return std::make_shared<AnimationGraph>(m_Graph);
}
