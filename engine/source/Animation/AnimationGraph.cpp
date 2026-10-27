#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Animation/Skeleton.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <algorithm>
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

int AnimationNode::FindState(std::string_view state) const
{
    for (size_t i = 0; i < states.size(); ++i)
        if (states[i] == state)
            return static_cast<int>(i);
    return -1;
}

int AnimationNode::AddState(std::string state, int node, glm::vec2 position)
{
    states.push_back(std::move(state));
    inputs.push_back(node);
    statePositions.resize(states.size() - 1, glm::vec2(0.0f));
    statePositions.push_back(position);
    return static_cast<int>(states.size()) - 1;
}

void AnimationNode::RemoveState(int state)
{
    if (state < 0 || state >= static_cast<int>(states.size()))
        return;
    states.erase(states.begin() + state);
    if (state < static_cast<int>(inputs.size()))
        inputs.erase(inputs.begin() + state);
    if (state < static_cast<int>(statePositions.size()))
        statePositions.erase(statePositions.begin() + state);
    transitions.erase(std::remove_if(transitions.begin(), transitions.end(), [&](const AnimationTransition& t) { return t.from == state || t.to == state; }), transitions.end());
    for (AnimationTransition& t : transitions)
    {
        if (t.from > state) --t.from;
        if (t.to > state) --t.to;
    }
    if (entryState > state || entryState >= static_cast<int>(states.size()))
        entryState = std::max(0, entryState - 1);
}

int AnimationGraph::AddNode(AnimationNode node)
{
    nodes.push_back(std::move(node));
    if (output < 0)
        output = static_cast<int>(nodes.size()) - 1;
    return static_cast<int>(nodes.size()) - 1;
}

void AnimationGraph::RemoveNode(int index)
{
    if (index < 0 || index >= static_cast<int>(nodes.size()))
        return;
    nodes.erase(nodes.begin() + index);
    auto renumber = [&](int& reference) {
        if (reference == index) reference = -1;
        else if (reference > index) --reference;
    };
    for (AnimationNode& node : nodes)
        for (int& input : node.inputs)
            renumber(input);
    renumber(output);
}

void AnimationGraph::RemoveParameter(int index)
{
    if (index < 0 || index >= static_cast<int>(parameters.size()))
        return;
    parameters.erase(parameters.begin() + index);
    auto renumber = [&](int& reference) {
        if (reference == index) reference = -1;
        else if (reference > index) --reference;
    };
    for (AnimationNode& node : nodes)
    {
        renumber(node.weight.parameter);
        renumber(node.parameter);
        renumber(node.parameterY);
        for (AnimationTransition& transition : node.transitions)
            for (AnimationCondition& condition : transition.conditions)
                renumber(condition.parameter);
    }
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
    auto vectorParameter = [&](int parameter) {
        return parameter >= 0 && parameter < static_cast<int>(parameters.size()) && parameters[parameter].type == AnimationParameterType::Vector;
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
                error = describe(i) + " has an input that isn't connected";
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
        case AnimationNodeType::Blend2D:
            inputs = node.inputs.size();
            if (inputs == 0 || node.positions.size() != inputs)
            {
                error = describe(i) + " needs a position per input";
                return false;
            }
            if (!floatParameter(node.parameter) || !floatParameter(node.parameterY))
            {
                error = describe(i) + " needs two float parameters";
                return false;
            }
            break;
        case AnimationNodeType::StateMachine:
        {
            inputs = node.states.size();
            const int states = static_cast<int>(inputs);
            if (states == 0 || node.inputs.size() != inputs)
            {
                error = describe(i) + " needs a node per state";
                return false;
            }
            if (node.entryState < 0 || node.entryState >= states)
            {
                error = describe(i) + " has no entry state";
                return false;
            }
            for (size_t t = 0; t < node.transitions.size(); ++t)
            {
                const AnimationTransition& transition = node.transitions[t];
                const std::string which = describe(i) + " transition " + std::to_string(t);
                if (transition.from < -1 || transition.from >= states || transition.to < 0 || transition.to >= states)
                {
                    error = which + " goes between states it doesn't have";
                    return false;
                }
                if (transition.conditions.empty() && transition.exitTime < 0.0f)
                {
                    error = which + " needs a condition or an exit time";
                    return false;
                }
                for (const AnimationCondition& condition : transition.conditions)
                {
                    if (condition.parameter < 0 || condition.parameter >= static_cast<int>(parameters.size()))
                    {
                        error = which + " tests a parameter that doesn't exist";
                        return false;
                    }
                    if (vectorParameter(condition.parameter))
                    {
                        error = which + " tests a vector parameter";
                        return false;
                    }
                }
            }
            break;
        }
        case AnimationNodeType::TwoBoneIK:
        case AnimationNodeType::LookAt:
            inputs = 1;
            if (node.bone.empty())
            {
                error = describe(i) + " names no bone";
                return false;
            }
            if (!vectorParameter(node.parameter))
            {
                error = describe(i) + " needs a vector parameter for its target";
                return false;
            }
            if (node.type == AnimationNodeType::TwoBoneIK && node.parameterY >= 0 && !vectorParameter(node.parameterY))
            {
                error = describe(i) + " has a pole that isn't a vector parameter";
                return false;
            }
            if (node.weight.parameter >= 0 && !floatParameter(node.weight.parameter))
            {
                error = describe(i) + " has a weight parameter that isn't a float";
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

int AnimationGraph::Builder::Vector(std::string name, glm::vec3 value)
{
    AnimationParameter parameter{ std::move(name), AnimationParameterType::Vector, 0.0f };
    parameter.defaultVector = value;
    m_Graph.parameters.push_back(std::move(parameter));
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

int AnimationGraph::Builder::Blend2D(std::string_view x, std::string_view y, std::vector<std::pair<int, glm::vec2>> inputs)
{
    AnimationNode node;
    node.type = AnimationNodeType::Blend2D;
    node.name = "Blend2D";
    node.parameter = Parameter(x).parameter;
    node.parameterY = Parameter(y).parameter;
    for (const auto& [input, position] : inputs)
    {
        node.inputs.push_back(input);
        node.positions.push_back(position);
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

int AnimationGraph::Builder::TwoBoneIK(int input, std::string bone, std::string_view target, std::string_view pole, AnimationWeight weight)
{
    AnimationNode node;
    node.type = AnimationNodeType::TwoBoneIK;
    node.name = "Two Bone IK";
    node.inputs = { input };
    node.bone = std::move(bone);
    node.parameter = Parameter(target).parameter;
    node.parameterY = pole.empty() ? -1 : Parameter(pole).parameter;
    node.weight = weight;
    return Add(std::move(node));
}

int AnimationGraph::Builder::LookAt(int input, std::string bone, std::string_view target, int chainLength, float angleLimit, AnimationWeight weight)
{
    AnimationNode node;
    node.type = AnimationNodeType::LookAt;
    node.name = "Look At";
    node.inputs = { input };
    node.bone = std::move(bone);
    node.parameter = Parameter(target).parameter;
    node.chainLength = chainLength;
    node.angleLimit = angleLimit;
    node.weight = weight;
    return Add(std::move(node));
}

int AnimationGraph::Builder::StateMachine(std::vector<std::pair<std::string, int>> states, int entryState)
{
    AnimationNode node;
    node.type = AnimationNodeType::StateMachine;
    node.name = "State Machine";
    node.entryState = entryState;
    for (auto& [state, input] : states)
    {
        node.states.push_back(std::move(state));
        node.inputs.push_back(input);
    }
    return Add(std::move(node));
}

AnimationTransition& AnimationGraph::Builder::Transition(int machine, std::string_view from, std::string_view to, float duration,
                                                         std::vector<AnimationCondition> conditions)
{
    static AnimationTransition unused;
    if (machine < 0 || machine >= static_cast<int>(m_Graph.nodes.size()) || m_Graph.nodes[machine].type != AnimationNodeType::StateMachine)
    {
        m_Error = "a transition was added to a node that isn't a state machine";
        return unused = {};
    }

    AnimationNode& node = m_Graph.nodes[machine];
    AnimationTransition transition;
    transition.from = from.empty() ? -1 : node.FindState(from);
    transition.to = node.FindState(to);
    transition.duration = duration;
    transition.conditions = std::move(conditions);
    if ((!from.empty() && transition.from < 0) || transition.to < 0)
    {
        m_Error = "a transition of " + node.name + " names a state it doesn't have (" + std::string(from) + " -> " + std::string(to) + ")";
        return unused = {};
    }
    node.transitions.push_back(std::move(transition));
    return node.transitions.back();
}

AnimationCondition AnimationGraph::Builder::When(std::string_view parameter, ConditionMode mode, float threshold) const
{
    const int index = m_Graph.FindParameter(parameter);
    if (index < 0)
        Logger::Warning("Animation graph: no parameter " + std::string(parameter));
    return { index, mode, threshold };
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

AnimationGraph::Builder& AnimationGraph::Builder::AddClip(std::string name, AssetHandle<AnimationClip> clip)
{
    m_Graph.clips.push_back({ std::move(name), std::move(clip) });
    return *this;
}

std::shared_ptr<AnimationGraph> AnimationGraph::Builder::Build()
{
    std::string error = m_Error;
    if (!error.empty() || !m_Graph.Validate(error))
    {
        Logger::Error("Animation graph: " + error);
        return nullptr;
    }
    return std::make_shared<AnimationGraph>(m_Graph);
}
