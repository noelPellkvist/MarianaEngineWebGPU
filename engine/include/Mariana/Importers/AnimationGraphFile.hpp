#pragma once
#include <filesystem>
#include <memory>
#include <string>

#include <Mariana/Assets/AssetManager.hpp>

class AnimationGraph;

// .animgraph files: an AnimationGraph as JSON.
//   {
//     "parameters": [ { "name": "speed", "type": "float", "default": 0 },
//                     { "name": "grounded", "type": "bool", "default": true }, { "name": "jump", "type": "trigger" } ],
//     "clips": [ { "name": "Run", "file": "../Models/Run.glb", "animation": 0 } ],
//     "nodes": [
//       { "name": "Idle", "type": "clip", "clip": "Idle", "speed": 1, "loop": true, "syncGroup": "move" },
//       { "name": "Move", "type": "blend1d", "parameter": "speed",
//         "inputs": [ { "node": "Idle", "threshold": 0 }, { "node": "Run", "threshold": 1 } ] },
//       { "name": "Aim", "type": "blend2d", "parameters": [ "x", "y" ],
//         "inputs": [ { "node": "Left", "position": [ -1, 0 ] }, { "node": "Right", "position": [ 1, 0 ] } ] },
//       { "name": "Mix", "type": "blend", "inputs": [ "A", "B" ], "weight": 0.5 },  (or "weight": "<float parameter>")
//       { "name": "Lean", "type": "additive", "inputs": [ "Move", "LeanClip" ], "weight": "lean", "reference": "firstFrame" | "rest" },
//       { "name": "Wave", "type": "layer", "inputs": [ "Move", "WaveClip" ], "mode": "override" | "additive",
//         "mask": [ { "bone": "Spine1", "weight": 1 } ], "weight": 1 },
//       { "name": "Body", "type": "stateMachine", "entry": "Move",
//         "states": [ { "name": "Move", "node": "Move" }, { "name": "Jump", "node": "JumpClip" } ],
//         "transitions": [ { "from": "Move", "to": "Jump", "duration": 0.1, "exitTime": 0.8, "interruptible": true,
//                            "conditions": [ { "parameter": "jump", "mode": "if" },
//                                            { "parameter": "speed", "mode": "greater", "threshold": 0.5 } ] } ] }
//     ],
//     "output": "Body"
//   }
// Nodes are referred to by name (or by index). A transition without "from" goes from any state.
// Condition modes: if, ifNot, greater, less, equals, notEquals. "clips" are the clips the graph
// brings (AnimationGraph::clips): a glTF file relative to the .animgraph (or a resource path
// starting with "/") and its animation by index or by "clip" name. Missing values keep defaults.
namespace AnimationGraphFile
{
    // Reads a file (an OS path) into a new graph, loading the clips it lists. nullptr (and `error`)
    // on failure; a graph that doesn't validate is a failure too.
    std::shared_ptr<AnimationGraph> Load(AssetManager2& assets, const std::filesystem::path& file, std::string& error);

    // Writes `graph` to `file`; each of its clips is stored by the glTF file it came from.
    bool Save(const AnimationGraph& graph, AssetManager2& assets, const std::filesystem::path& file, std::string& error);

    // Makes AssetManager2::Load<AnimationGraph>() handle .animgraph files.
    void RegisterLoader(AssetManager2& assets);
}
