#pragma once
// Scene files with known answers: what the editor shows comes back from a saved file (an Animator's
// graph parameters and state, the lighting and pipeline, a model's material edited in the scene) and
// saving it again writes the same file; the signature changes with any edit and only then; one
// entity's components put back after the scene was restored (keeping play-mode changes). On the
// Sandbox's BoxAnimated.glb (no textures, so no GPU is needed), files written to a temporary folder.
#include "TestCommon.hpp"

#include <Mariana/Animation/AnimationGraph.hpp>
#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Importers/AnimationGraphFile.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Importers/SceneFile.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Scene/Prefab.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace EngineScenarios
{
    namespace detail
    {
        inline std::string ReadText(const std::filesystem::path& file)
        {
            std::ifstream stream(file, std::ios::binary);
            std::ostringstream text;
            text << stream.rdbuf();
            return text.str();
        }

        // A project folder of the test's own (a FileSystem root before the Sandbox's), emptied once.
        inline const std::filesystem::path& SceneTestFolder()
        {
            static const std::filesystem::path folder = [] {
                const std::filesystem::path f = std::filesystem::temp_directory_path() / "MarianaSceneTest";
                std::error_code ec;
                std::filesystem::remove_all(f, ec);
                std::filesystem::create_directories(f / "Scenes", ec);
                std::filesystem::create_directories(f / "Animations", ec);
                FileSystem::AddRoot(MARIANA_SANDBOX_DIR);
                FileSystem::AddRoot(f);
                return f;
            }();
            return folder;
        }

        struct SceneWorld
        {
            AssetManager2 assets; // outlives the scene, whose components hold handles
            Scene scene;
            System animation;

            SceneWorld()
            {
                SceneTestFolder();
                scene.UpdateComponentRegistry();
                GLTF::GLTFLoader::RegisterLoader(assets);
                MaterialFile::RegisterLoader(assets);
                AnimationGraphFile::RegisterLoader(assets);
                animation = Animator::CreateSystem(scene);
            }

            // Animators bind (and pose), transforms follow; no time passes.
            void Update()
            {
                animation.Run(0.0f);
                scene.Update(0.0f);
            }

            Entity Box() const { return scene.FindByName("Box"); }

            // The box's first mesh's first material.
            MaterialAsset* BoxMaterial() const
            {
                MaterialAsset* found = nullptr;
                scene.Each<MeshComponent>([&](Entity, const MeshComponent& mesh) {
                    if (!found && !mesh.materials.empty())
                        found = mesh.materials[0].Get().get();
                });
                return found;
            }
        };

        // BoxAnimated playing a graph of its clip (two states, three parameters), its graph saved
        // to the test's folder so the scene refers to the file.
        inline bool BuildBoxScene(SceneWorld& w, std::string& error)
        {
            AssetHandle<Prefab> prefab = w.assets.Load<Prefab>(FileSystem::Resolve("/Models/BoxAnimated.glb"));
            if (!prefab.Get())
            {
                error = "no /Models/BoxAnimated.glb";
                return false;
            }
            Entity box = w.scene.Instantiate(*prefab.Get(), "Box");
            Animator* animator = box.Get<Animator>();
            if (!animator || animator->clips.empty())
            {
                error = "BoxAnimated has no Animator";
                return false;
            }
            AnimationGraph::Builder b;
            b.Float("speed", 1.0f);
            b.Bool("happy");
            b.Vector("target");
            const int idle = b.Clip(animator->clips[0].name);
            const int move = b.Clip(animator->clips[0].name);
            b.Output(b.StateMachine({ { "Idle", idle }, { "Move", move } }));
            const std::shared_ptr<AnimationGraph> graph = b.Build();
            const std::filesystem::path file = SceneTestFolder() / "Animations" / "Box.animgraph";
            if (!graph || !AnimationGraphFile::Save(*graph, w.assets, file, error))
                return false;
            animator->graph = w.assets.Load<AnimationGraph>(file);
            w.Update();
            return animator->graph.Get() != nullptr;
        }

        inline float Distance(const glm::vec3& a, const glm::vec3& b) { return glm::length(a - b); }
    }

    // Saved, everything unloaded, loaded and saved again: the Animator's values, the settings and the
    // model's edited material come back, and the second file is the first.
    inline std::vector<Result> SceneRoundTrip(const Log& log)
    {
        using namespace detail;
        std::vector<Result> results;
        SceneWorld w;
        std::string error;
        if (!BuildBoxScene(w, error))
        {
            results.push_back(Finish(log, "scene round trip", error, false));
            return results;
        }

        Animator& animator = *w.Box().Get<Animator>();
        animator.SetFloat("speed", 2.5f);
        animator.SetBool("happy", true);
        animator.SetVector("target", glm::vec3(1.0f, 2.0f, 3.0f));
        animator.rootMotion = true;
        animator.CrossFadeState("Move", 0.0f);
        MaterialAsset* material = w.BoxMaterial();
        if (!material)
        {
            results.push_back(Finish(log, "scene round trip", "the box has no material", false));
            return results;
        }
        material->properties.baseColor = glm::vec4(0.2f, 0.4f, 0.6f, 1.0f);
        material->properties.roughnessFactor = 0.3f;
        material->MarkDirty();
        w.Update();

        SceneSettings settings;
        settings.lighting.sunAzimuth = 123.0f;
        settings.lighting.sunColor = glm::vec3(1.0f, 0.9f, 0.8f);
        settings.lighting.skyRotation = 45.0f;
        settings.pipeline = "/Pipelines/Test.pipeline";
        const std::filesystem::path first = SceneTestFolder() / "Scenes" / "RoundTrip.scene";
        const std::filesystem::path second = SceneTestFolder() / "Scenes" / "RoundTrip2.scene";
        if (!SceneFile::Save(w.scene, w.assets, first, error, &settings))
        {
            results.push_back(Finish(log, "scene round trip", error, false));
            return results;
        }

        // Everything goes, the model too, so its material is read from its file again.
        w.scene.Clear();
        const size_t unloaded = w.assets.UnloadUnused();
        SceneSettings loaded;
        std::string warnings;
        const bool read = SceneFile::Load(w.scene, w.assets, first, error, &warnings, &loaded);
        w.Update();
        Entity box = w.Box();
        const Animator* back = box.IsValid() ? box.TryGet<Animator>() : nullptr;
        if (!read || !back)
        {
            results.push_back(Finish(log, "scene round trip", read ? "no box after loading" : error, false));
            return results;
        }

        const std::vector<std::string> states = back->CurrentStates();
        const std::string state = states.empty() ? std::string("(entry)") : states[0];
        const glm::vec3 target = back->GetVector("target");
        results.push_back(Finish(log, "scene keeps an animator's values",
                                 Format("speed %.2f, happy %.0f, target (%.1f, %.1f, %.1f)", back->GetFloat("speed"), back->GetBool("happy") ? 1 : 0, target.x, target.y, target.z) +
                                     ", state " + state + (back->rootMotion ? ", root motion" : ""),
                                 back->GetFloat("speed") == 2.5f && back->GetBool("happy") && target == glm::vec3(1.0f, 2.0f, 3.0f) && state == "Move" && back->rootMotion));

        results.push_back(Finish(log, "scene keeps its settings",
                                 Format("sun azimuth %.1f, color (%.2f, %.2f, %.2f), sky turned %.1f", loaded.lighting.sunAzimuth, loaded.lighting.sunColor.r,
                                        loaded.lighting.sunColor.g, loaded.lighting.sunColor.b, loaded.lighting.skyRotation) + ", pipeline " + loaded.pipeline,
                                 loaded.hasLighting && loaded.lighting.sunAzimuth == 123.0f && loaded.lighting.sunColor == glm::vec3(1.0f, 0.9f, 0.8f) &&
                                     loaded.lighting.skyRotation == 45.0f && loaded.pipeline == "/Pipelines/Test.pipeline"));

        const MaterialAsset* edited = w.BoxMaterial();
        const glm::vec4 color = edited ? edited->properties.baseColor : glm::vec4(0.0f);
        results.push_back(Finish(log, "scene keeps a model's material edit",
                                 Format("%.0f assets unloaded; base color (%.2f, %.2f, %.2f), roughness %.2f", static_cast<double>(unloaded), color.r, color.g, color.b,
                                        edited ? edited->properties.roughnessFactor : 0.0f),
                                 unloaded > 0 && edited && Distance(glm::vec3(color), glm::vec3(0.2f, 0.4f, 0.6f)) < 1e-6f && edited->properties.roughnessFactor == 0.3f));

        if (!SceneFile::Save(w.scene, w.assets, second, error, &loaded))
        {
            results.push_back(Finish(log, "scene saved again", error, false));
            return results;
        }
        const std::string a = ReadText(first), b = ReadText(second);
        results.push_back(Finish(log, "scene saved again is the same file",
                                 Format("%.0f and %.0f bytes", static_cast<double>(a.size()), static_cast<double>(b.size())) + (warnings.empty() ? "" : " | " + warnings),
                                 !a.empty() && a == b && a.find("materialOverrides") != std::string::npos && a.find("\"states\"") != std::string::npos));
        return results;
    }

    // The signature stays while nothing changes, and any change to what a file keeps changes it: a
    // position (back again, it is the same), a material the scene holds, a parent, a clip's events,
    // the settings.
    inline Result SceneSignature(const Log& log)
    {
        using namespace detail;
        SceneWorld w;
        std::string error;
        if (!BuildBoxScene(w, error))
            return Finish(log, "scene signature", error, false);
        SceneSettings settings;
        auto signature = [&]() { return SceneFile::Signature(w.scene, w.assets, &settings); };

        const uint64_t start = signature();
        const bool steady = signature() == start;
        Entity box = w.Box();
        const LocalTRS before = *box.TryGet<LocalTRS>();
        box.SetPosition(before.pos[0] + 1.0f, before.pos[1], before.pos[2]);
        const bool moved = signature() != start;
        box.SetPosition(before.pos[0], before.pos[1], before.pos[2]);
        const bool back = signature() == start;

        MaterialAsset* material = w.BoxMaterial();
        const glm::vec4 color = material->properties.baseColor;
        material->properties.baseColor.g += 0.25f;
        material->MarkDirty();
        const bool tinted = signature() != start;
        material->properties.baseColor = color;
        material->MarkDirty();
        const bool untinted = signature() == start;

        box.Get<Animator>()->clips[0].events.push_back({ 0.5f, "Step" });
        const bool evented = signature() != start;
        box.Get<Animator>()->clips[0].events.clear();

        settings.lighting.sunElevation += 10.0f;
        const bool lit = signature() != start;
        settings = SceneSettings{};

        Entity other = w.scene.Instantiate("Other");
        const uint64_t withOther = signature();
        Entity child;
        w.scene.ForEachDescendant(box, [&](Entity e) {
            if (!child.IsValid())
                child = e;
        });
        child.SetParent(other);
        const bool reparented = signature() != withOther;

        return Finish(log, "scene signature", Format("steady %.0f, moved %.0f, moved back %.0f, material %.0f / back %.0f", steady, moved, back, tinted, untinted) +
                                                  Format(", events %.0f, lighting %.0f, parent %.0f", evented, lit, reparented),
                      steady && moved && back && tinted && untinted && evented && lit && reparented);
    }

    // Keeping play-mode changes: an entity's components saved while the game runs, the scene put back
    // as it was on Play, then the saved components given back to the entity (and not to the others).
    inline Result SceneKeepEntity(const Log& log)
    {
        using namespace detail;
        SceneWorld w;
        std::string error;
        if (!BuildBoxScene(w, error))
            return Finish(log, "keep an entity's changes", error, false);
        Entity box = w.Box();
        Entity other = w.scene.Instantiate("Other");
        other.SetPosition(0.0f, 0.0f, 0.0f);
        const SceneSnapshot snapshot = w.scene.Snapshot();

        // "Playing".
        box.SetPosition(5.0f, 1.0f, -2.0f);
        box.Get<Animator>()->speed = 3.0f;
        box.Get<Animator>()->SetFloat("speed", 4.0f);
        other.SetPosition(9.0f, 9.0f, 9.0f);
        const SceneFile::EntityState kept = SceneFile::SaveEntity(box, w.assets);

        w.scene.Restore(snapshot);
        const LocalTRS* restored = box.TryGet<LocalTRS>();
        const bool reset = restored && restored->pos[0] == 0.0f && box.TryGet<Animator>()->speed == 1.0f;
        SceneFile::LoadEntity(box, w.assets, kept);
        const LocalTRS* t = box.TryGet<LocalTRS>();
        const Animator* animator = box.TryGet<Animator>();
        const LocalTRS* o = other.TryGet<LocalTRS>();
        const glm::vec3 position = t ? glm::vec3(t->pos[0], t->pos[1], t->pos[2]) : glm::vec3(0.0f);
        return Finish(log, "keep an entity's changes",
                      Format("reset %.0f; kept at (%.1f, %.1f, %.1f), speed %.1f", reset, position.x, position.y, position.z, animator ? animator->speed : 0.0f) +
                          Format(", graph speed %.1f; the other at x %.1f", animator ? animator->GetFloat("speed") : 0.0f, o ? o->pos[0] : -1.0f),
                      reset && position == glm::vec3(5.0f, 1.0f, -2.0f) && animator && animator->speed == 3.0f && animator->GetFloat("speed") == 4.0f &&
                          animator->graph.Get() && o && o->pos[0] == 0.0f);
    }
}
