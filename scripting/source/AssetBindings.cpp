// Meshes, materials and textures from scripts: the mesh, material and texture libraries, their
// userdata (Mesh, Material, Texture) and the entity methods that put them on an entity.
//
// Materials a script changes are always its own: material.new and Clone make new ones, and
// entity:GetMaterial gives the entity a copy the first time, so the edited scene's materials and
// those models share are never touched (Stop puts the entity's Mesh Renderer back as it was).
#include "Bindings.hpp"

#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Geometry/MeshBuilder.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Importers/ImageImporter.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace ScriptingInternal
{
    // A mesh as scripts know it: the engine's mesh and, for a dynamic one a script made, what it was
    // made from (Update builds it again from that with the new values).
    struct ScriptMesh
    {
        AssetHandle<IMesh> handle;
        bool made = false;
        bool dynamic = false;
        SourceTopology topology = SourceTopology::Triangles;
        bool givenNormals = false;
        bool smooth = false;
        size_t vertexCount = 0;
        std::vector<glm::vec3> positions, normals, colors;
        std::vector<glm::vec2> uvs;
        std::vector<std::vector<uint32_t>> groups; // indices per submesh (from 0); empty = in order
    };

    namespace
    {
        struct MeshRef
        {
            std::shared_ptr<ScriptMesh> mesh;
        };

        struct MaterialRef
        {
            AssetHandle<MaterialAsset> material;
        };

        // An image file, loaded in the colour space of the slot it is put in (colours sRGB, data linear).
        struct TextureRef
        {
            std::string path;
            std::filesystem::path file;
            AssetHandle<Texture> srgb, linear;
        };

        template <typename T>
        void DestroyUserdata(lua_State*, void* data)
        {
            static_cast<T*>(data)->~T();
        }

        template <typename T>
        T* NewUserdata(lua_State* L, int tag)
        {
            return new (lua_newuserdatataggedwithmetatable(L, sizeof(T), tag)) T();
        }

        template <typename T>
        T* CheckUserdata(lua_State* L, int index, int tag, const char* type)
        {
            T* data = static_cast<T*>(lua_touserdatatagged(L, index, tag));
            if (!data)
                luaL_typeerror(L, index, type);
            return data;
        }

        AssetManager2& Assets(lua_State* L) { return *GetContext(L).assets; }

        // ---- Mesh data ------------------------------------------------------------------------

        // A vertex attribute from the table at `table`: a table of vectors, or a buffer of
        // `components` 32-bit floats per vertex. False when the field isn't there.
        bool ReadVectors(lua_State* L, int table, const char* field, int components, std::vector<glm::vec3>& out)
        {
            lua_getfield(L, table, field);
            const int index = lua_gettop(L);
            out.clear();
            if (lua_isnil(L, index))
            {
                lua_pop(L, 1);
                return false;
            }
            if (lua_isbuffer(L, index))
            {
                size_t bytes = 0;
                const auto* data = static_cast<const char*>(lua_tobuffer(L, index, &bytes));
                const size_t stride = static_cast<size_t>(components) * sizeof(float);
                if (bytes % stride != 0)
                    Error(L, "%s: a buffer holds %d floats (%d bytes) per vertex, but has %d bytes", field, components, static_cast<int>(stride),
                          static_cast<int>(bytes));
                out.resize(bytes / stride);
                for (size_t i = 0; i < out.size(); ++i)
                {
                    float v[3] = { 0.0f, 0.0f, 0.0f };
                    std::memcpy(v, data + i * stride, stride);
                    out[i] = glm::vec3(v[0], v[1], v[2]);
                }
            }
            else if (lua_istable(L, index))
            {
                const int n = lua_objlen(L, index);
                out.resize(static_cast<size_t>(n));
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, index, i);
                    if (!lua_isvector(L, -1))
                        Error(L, "%s[%d] should be a vector, got %s", field, i, luaL_typename(L, -1));
                    const float* v = lua_tovector(L, -1);
                    out[static_cast<size_t>(i - 1)] = glm::vec3(v[0], v[1], v[2]);
                    lua_pop(L, 1);
                }
            }
            else
                Error(L, "%s should be a table of vectors or a buffer, got %s", field, luaL_typename(L, index));
            lua_pop(L, 1);
            return true;
        }

        // Indices from a table (counting from 1, like Luau's tables) or a buffer of 32-bit integers
        // (counting from 0), each naming one of the vertices.
        std::vector<uint32_t> ReadIndices(lua_State* L, int index, const char* field, size_t vertexCount)
        {
            std::vector<uint32_t> out;
            if (lua_isbuffer(L, index))
            {
                size_t bytes = 0;
                const void* data = lua_tobuffer(L, index, &bytes);
                if (bytes % sizeof(uint32_t) != 0)
                    Error(L, "%s: a buffer of indices holds 32-bit integers, but has %d bytes", field, static_cast<int>(bytes));
                out.resize(bytes / sizeof(uint32_t));
                std::memcpy(out.data(), data, bytes);
                for (size_t i = 0; i < out.size(); ++i)
                    if (out[i] >= vertexCount)
                        Error(L, "%s: index %d is %d, but buffers count vertices from 0 to %d", field, static_cast<int>(i), static_cast<int>(out[i]),
                              static_cast<int>(vertexCount) - 1);
                return out;
            }
            if (!lua_istable(L, index))
                Error(L, "%s should be a table of numbers or a buffer, got %s", field, luaL_typename(L, index));
            const int n = lua_objlen(L, index);
            out.reserve(static_cast<size_t>(n));
            for (int i = 1; i <= n; ++i)
            {
                lua_rawgeti(L, index, i);
                if (lua_type(L, -1) != LUA_TNUMBER)
                    Error(L, "%s[%d] should be a number, got %s", field, i, luaL_typename(L, -1));
                const double v = lua_tonumber(L, -1);
                if (v < 1.0 || v > static_cast<double>(vertexCount) || std::floor(v) != v)
                    Error(L, "%s[%d] is %g: indices go from 1 to the number of positions (%d)", field, i, v, static_cast<int>(vertexCount));
                out.push_back(static_cast<uint32_t>(v) - 1u);
                lua_pop(L, 1);
            }
            return out;
        }

        void CheckCount(lua_State* L, SourceTopology topology, size_t count, const char* what)
        {
            if (topology == SourceTopology::Triangles && count % 3 != 0)
                Error(L, "%s: triangles take three indices each, but there are %d", what, static_cast<int>(count));
            if (topology == SourceTopology::Lines && count % 2 != 0)
                Error(L, "%s: lines take two indices each, but there are %d", what, static_cast<int>(count));
        }

        // Reads mesh.new's table (or Update's: only vertex attributes, as many as before) into `m`.
        void ReadMesh(lua_State* L, int table, ScriptMesh& m, bool update)
        {
            table = lua_absindex(L, table);
            static const char* const kFields[] = { "positions", "normals", "uvs", "colors", "indices", "submeshes", "topology", "smooth", "dynamic" };
            lua_pushnil(L);
            while (lua_next(L, table) != 0)
            {
                const char* key = lua_type(L, -2) == LUA_TSTRING ? lua_tostring(L, -2) : nullptr;
                bool known = false;
                for (int i = 0; i < (update ? 4 : 9) && key; ++i)
                    known = known || std::strcmp(key, kFields[i]) == 0;
                if (!known)
                    Error(L, update ? "'%s' can't be updated: Update takes positions, normals, uvs and colors"
                                    : "'%s' isn't a mesh field (positions, normals, uvs, colors, indices, submeshes, topology, smooth, dynamic)",
                          key ? key : "?");
                lua_pop(L, 1);
            }

            std::vector<glm::vec3> positions;
            if (ReadVectors(L, table, "positions", 3, positions))
            {
                if (update && positions.size() != m.positions.size())
                    Error(L, "positions: an update keeps the mesh's %d positions, got %d", static_cast<int>(m.positions.size()),
                          static_cast<int>(positions.size()));
                m.positions = std::move(positions);
            }
            else if (!update)
                Error(L, "a mesh needs positions");
            const size_t n = m.positions.size();
            auto attribute = [&](const char* field, int components, std::vector<glm::vec3>& target) {
                std::vector<glm::vec3> values;
                if (!ReadVectors(L, table, field, components, values))
                    return false;
                if (values.size() != n)
                    Error(L, "%s has %d values for %d positions", field, static_cast<int>(values.size()), static_cast<int>(n));
                target = std::move(values);
                return true;
            };
            if (attribute("normals", 3, m.normals))
                m.givenNormals = true;
            if (std::vector<glm::vec3> uvs; attribute("uvs", 2, uvs))
            {
                m.uvs.clear();
                for (const glm::vec3& uv : uvs)
                    m.uvs.emplace_back(uv.x, uv.y);
            }
            attribute("colors", 3, m.colors);
            if (update)
                return;

            lua_getfield(L, table, "topology");
            if (!lua_isnil(L, -1))
            {
                const char* topology = lua_tostring(L, -1);
                if (topology && std::strcmp(topology, "triangles") == 0)
                    m.topology = SourceTopology::Triangles;
                else if (topology && std::strcmp(topology, "lines") == 0)
                    m.topology = SourceTopology::Lines;
                else if (topology && std::strcmp(topology, "points") == 0)
                    m.topology = SourceTopology::Points;
                else
                    Error(L, "topology is \"triangles\", \"lines\" or \"points\"");
            }
            lua_pop(L, 1);
            lua_getfield(L, table, "smooth");
            m.smooth = lua_toboolean(L, -1) != 0;
            lua_getfield(L, table, "dynamic");
            m.dynamic = lua_toboolean(L, -1) != 0;
            lua_pop(L, 2);

            m.groups.clear();
            lua_getfield(L, table, "submeshes");
            if (!lua_isnil(L, -1))
            {
                if (!lua_istable(L, -1))
                    Error(L, "submeshes should be a table of index lists");
                const int count = lua_objlen(L, -1);
                for (int i = 1; i <= count; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    char field[32];
                    std::snprintf(field, sizeof(field), "submeshes[%d]", i);
                    m.groups.push_back(ReadIndices(L, -1, field, n));
                    CheckCount(L, m.topology, m.groups.back().size(), field);
                    lua_pop(L, 1);
                }
                if (m.groups.empty())
                    Error(L, "submeshes is empty");
            }
            lua_pop(L, 1);
            if (m.groups.empty())
            {
                lua_getfield(L, table, "indices");
                if (lua_isnil(L, -1))
                {
                    m.groups.emplace_back(); // in order
                    CheckCount(L, m.topology, n, "positions (without indices, in order)");
                }
                else
                {
                    m.groups.push_back(ReadIndices(L, -1, "indices", n));
                    CheckCount(L, m.topology, m.groups.back().size(), "indices");
                }
                lua_pop(L, 1);
            }
        }

        // Normals shared at each vertex: the faces around it, weighted by their area.
        std::vector<glm::vec3> SmoothNormals(const ScriptMesh& m)
        {
            std::vector<glm::vec3> normals(m.positions.size(), glm::vec3(0.0f));
            auto face = [&](uint32_t a, uint32_t b, uint32_t c) {
                const glm::vec3 n = glm::cross(m.positions[b] - m.positions[a], m.positions[c] - m.positions[a]);
                normals[a] += n;
                normals[b] += n;
                normals[c] += n;
            };
            for (const std::vector<uint32_t>& group : m.groups)
            {
                if (group.empty())
                    for (uint32_t i = 0; i + 2 < m.positions.size(); i += 3)
                        face(i, i + 1, i + 2);
                else
                    for (size_t i = 0; i + 2 < group.size(); i += 3)
                        face(group[i], group[i + 1], group[i + 2]);
            }
            for (glm::vec3& n : normals)
            {
                const float length = glm::length(n);
                n = length > 1e-12f ? n / length : glm::vec3(0.0f, 1.0f, 0.0f);
            }
            return normals;
        }

        // A section per submesh, each with every vertex and its own indices.
        std::vector<MeshSection> Sections(const ScriptMesh& m)
        {
            const std::vector<glm::vec3> normals = m.givenNormals ? m.normals
                                                   : m.smooth && m.topology == SourceTopology::Triangles ? SmoothNormals(m)
                                                                                                          : std::vector<glm::vec3>();
            const bool hasNormals = !normals.empty();
            std::vector<StandardVertex> vertices(m.positions.size(), StandardVertex{});
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                StandardVertex& v = vertices[i];
                v.position = m.positions[i];
                v.normal = hasNormals ? normals[i] : glm::vec3(0.0f);
                v.texcoord0 = m.uvs.empty() ? glm::vec2(0.0f) : m.uvs[i];
                v.color0 = glm::vec4(m.colors.empty() ? glm::vec3(1.0f) : m.colors[i], 1.0f);
            }
            std::vector<MeshSection> sections;
            for (size_t g = 0; g < m.groups.size(); ++g)
            {
                MeshSection section;
                section.vertices = vertices;
                section.indices = m.groups[g];
                section.topology = m.topology;
                section.materialIndex = static_cast<uint32_t>(g);
                section.hasNormals = hasNormals;
                sections.push_back(std::move(section));
            }
            return sections;
        }

        // Makes the engine's mesh, or for an update writes its vertices again.
        void Upload(lua_State* L, ScriptMesh& m)
        {
            MeshData data = PrepareMesh(Sections(m));
            if (data.Empty())
                Error(L, "the mesh has nothing to draw (no whole triangle, line or point)");
            if (m.handle.IsValid() && m.handle.Get())
            {
                IMesh& mesh = *m.handle.Get();
                Bounds bounds;
                for (const StandardVertex& v : data.vertices)
                    bounds.Add(v.position);
                if (data.indices.size() != mesh.IndexCount() || !mesh.WriteVertices(data.vertices.data(), data.vertices.size(), sizeof(StandardVertex), bounds))
                    Error(L, "an update must keep the mesh's shape: make a new mesh for another");
                return;
            }
            std::shared_ptr<IMesh> mesh = UploadMesh(std::move(data), m.dynamic ? MeshUsage::Dynamic : MeshUsage::Static);
            if (!mesh)
                Error(L, "the mesh has nothing to draw");
            m.handle = Assets(L).RegisterAsset<IMesh>(std::move(mesh), AssetType::Mesh, m.dynamic ? "Script mesh (dynamic)" : "Script mesh");
        }

        // Dynamic meshes scripts made that nothing uses any more (no entity, no Luau value).
        void Prune(Context& context)
        {
            for (auto it = context.scriptMeshes.begin(); it != context.scriptMeshes.end();)
            {
                const AssetMetadata* meta = it->second->handle.GetMetadata();
                if (it->second.use_count() == 1 && (!meta || meta->refCount <= 1))
                    it = context.scriptMeshes.erase(it);
                else
                    ++it;
            }
        }

        void PushMesh(lua_State* L, std::shared_ptr<ScriptMesh> mesh)
        {
            NewUserdata<MeshRef>(L, kMeshTag)->mesh = std::move(mesh);
        }

        ScriptMesh& CheckMesh(lua_State* L, int index)
        {
            return *CheckUserdata<MeshRef>(L, index, kMeshTag, "Mesh")->mesh;
        }

        // mesh.new{ positions = ..., normals, uvs, colors, indices | submeshes, topology, smooth, dynamic }
        int MeshNew(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            auto m = std::make_shared<ScriptMesh>();
            m->made = true;
            ReadMesh(L, 1, *m, false);
            m->vertexCount = m->positions.size();
            Upload(L, *m);
            Context& context = GetContext(L);
            Prune(context);
            if (m->dynamic)
                context.scriptMeshes[m->handle.Get().get()] = m;
            else
            {
                // Only an update needs them.
                m->positions = {};
                m->normals = {};
                m->colors = {};
                m->uvs = {};
                m->groups = {};
            }
            PushMesh(L, std::move(m));
            return 1;
        }

        int MeshUpdate(lua_State* L)
        {
            ScriptMesh& m = CheckMesh(L, 1);
            luaL_checktype(L, 2, LUA_TTABLE);
            if (!m.made || !m.dynamic)
                Error(L, "only a dynamic mesh can be updated: make it with mesh.new{ ..., dynamic = true }");
            ReadMesh(L, 2, m, true);
            Upload(L, m);
            return 0;
        }

        int MeshIndex(lua_State* L)
        {
            ScriptMesh& m = CheckMesh(L, 1);
            const char* key = luaL_checkstring(L, 2);
            const IMesh* mesh = m.handle.Get().get();
            if (std::strcmp(key, "vertexCount") == 0)
                lua_pushnumber(L, static_cast<double>(m.made ? m.vertexCount : (mesh ? mesh->VertexCount() : 0)));
            else if (std::strcmp(key, "boundsMin") == 0)
                PushVector(L, mesh ? mesh->boundsMin : glm::vec3(0.0f));
            else if (std::strcmp(key, "boundsMax") == 0)
                PushVector(L, mesh ? mesh->boundsMax : glm::vec3(0.0f));
            else if (std::strcmp(key, "dynamic") == 0)
                lua_pushboolean(L, m.made && m.dynamic);
            else if (std::strcmp(key, "Update") == 0)
                lua_pushcfunction(L, MeshUpdate, "Update");
            else
                Error(L, "Mesh has no member '%s'", key);
            return 1;
        }

        int MeshNamecall(lua_State* L)
        {
            const char* name = lua_namecallatom(L, nullptr);
            if (name && std::strcmp(name, "Update") == 0)
                return MeshUpdate(L);
            Error(L, "Mesh has no method '%s'", name ? name : "?");
        }

        int MeshEq(lua_State* L)
        {
            const auto* a = static_cast<const MeshRef*>(lua_touserdatatagged(L, 1, kMeshTag));
            const auto* b = static_cast<const MeshRef*>(lua_touserdatatagged(L, 2, kMeshTag));
            lua_pushboolean(L, a && b && a->mesh->handle.Get() == b->mesh->handle.Get());
            return 1;
        }

        int MeshToString(lua_State* L)
        {
            const auto* ref = static_cast<const MeshRef*>(lua_touserdatatagged(L, 1, kMeshTag));
            const IMesh* mesh = ref ? ref->mesh->handle.Get().get() : nullptr;
            char text[96];
            std::snprintf(text, sizeof(text), "Mesh(%d vertices%s)", mesh ? static_cast<int>(mesh->VertexCount()) : 0,
                          ref && ref->mesh->dynamic ? ", dynamic" : "");
            lua_pushstring(L, text);
            return 1;
        }

        // ---- Textures -------------------------------------------------------------------------

        TextureRef& CheckTexture(lua_State* L, int index)
        {
            return *CheckUserdata<TextureRef>(L, index, kTextureTag, "Texture");
        }

        // The texture in the colour space a slot reads it in, loaded the first time.
        AssetHandle<Texture> TextureFor(lua_State* L, TextureRef& texture, bool srgb)
        {
            AssetHandle<Texture>& handle = srgb ? texture.srgb : texture.linear;
            if (!handle.IsValid())
            {
                handle = ImageImporter::LoadTexture(Assets(L), texture.file, srgb);
                if (!handle.IsValid())
                    Error(L, "could not load the image %s", texture.path.c_str());
            }
            return handle;
        }

        // texture.load(path): an image by resource path ("/Textures/Grass.png").
        int TextureLoad(lua_State* L)
        {
            const char* path = luaL_checkstring(L, 1);
            const std::filesystem::path file = FileSystem::Resolve(path);
            if (file.empty())
                Error(L, "no file %s", path);
            TextureRef* texture = NewUserdata<TextureRef>(L, kTextureTag);
            texture->path = path;
            texture->file = file;
            return 1;
        }

        void PushTexture(lua_State* L, const AssetHandle<Texture>& handle, bool srgb)
        {
            const AssetMetadata* meta = handle.GetMetadata();
            const std::filesystem::path file = meta ? (!meta->source.empty() ? meta->source : meta->path) : std::filesystem::path();
            TextureRef* texture = NewUserdata<TextureRef>(L, kTextureTag);
            texture->file = file;
            texture->path = FileSystem::ToResourcePath(file);
            if (texture->path.empty())
                texture->path = meta ? meta->name : std::string();
            (srgb ? texture->srgb : texture->linear) = handle;
        }

        int TextureIndex(lua_State* L)
        {
            TextureRef& texture = CheckTexture(L, 1);
            const char* key = luaL_checkstring(L, 2);
            if (std::strcmp(key, "path") != 0)
                Error(L, "Texture has no member '%s'", key);
            lua_pushstring(L, texture.path.c_str());
            return 1;
        }

        int TextureEq(lua_State* L)
        {
            const auto* a = static_cast<const TextureRef*>(lua_touserdatatagged(L, 1, kTextureTag));
            const auto* b = static_cast<const TextureRef*>(lua_touserdatatagged(L, 2, kTextureTag));
            lua_pushboolean(L, a && b && a->file == b->file);
            return 1;
        }

        int TextureToString(lua_State* L)
        {
            const auto* texture = static_cast<const TextureRef*>(lua_touserdatatagged(L, 1, kTextureTag));
            lua_pushstring(L, ("Texture(" + (texture ? texture->path : std::string("?")) + ")").c_str());
            return 1;
        }

        // ---- Materials ------------------------------------------------------------------------

        AssetHandle<MaterialAsset> RegisterMaterial(lua_State* L, std::shared_ptr<MaterialAsset> material, const std::string& name)
        {
            Context& context = GetContext(L);
            AssetHandle<MaterialAsset> handle = context.assets->RegisterAsset<MaterialAsset>(std::move(material), AssetType::Material, name);
            context.scriptMaterials.insert(handle.GetID());
            return handle;
        }

        void PushMaterial(lua_State* L, const AssetHandle<MaterialAsset>& material)
        {
            NewUserdata<MaterialRef>(L, kMaterialTag)->material = material;
        }

        MaterialAsset& CheckMaterial(lua_State* L, int index, AssetHandle<MaterialAsset>* handle = nullptr)
        {
            MaterialRef* ref = CheckUserdata<MaterialRef>(L, index, kMaterialTag, "Material");
            if (!ref->material.Get())
                Error(L, "the material is gone");
            if (handle)
                *handle = ref->material;
            return *ref->material.Get();
        }

        struct MapField
        {
            const char* name;
            MaterialTexture slot;
            bool srgb;
        };
        constexpr MapField kMaps[] = {
            { "baseColorMap", MaterialTexture::BaseColor, true },
            { "normalMap", MaterialTexture::Normal, false },
            { "metallicRoughnessMap", MaterialTexture::MetallicRoughness, false },
            { "occlusionMap", MaterialTexture::Occlusion, false },
            { "emissiveMap", MaterialTexture::Emissive, true },
        };
        const char* const kAlphaModes[] = { "Opaque", "Mask", "Blend", nullptr };

        float NumberValue(lua_State* L, int index, const char* name)
        {
            if (lua_type(L, index) != LUA_TNUMBER)
                Error(L, "%s expects a number, got %s", name, luaL_typename(L, index));
            return static_cast<float>(lua_tonumber(L, index));
        }

        glm::vec3 VectorValue(lua_State* L, int index, const char* name)
        {
            if (!lua_isvector(L, index))
                Error(L, "%s expects a vector, got %s", name, luaL_typename(L, index));
            const float* v = lua_tovector(L, index);
            return { v[0], v[1], v[2] };
        }

        // Sets one of a material's fields from the value at `index`; false when there is no such field.
        bool SetMaterialField(lua_State* L, MaterialAsset& m, const char* name, int index)
        {
            PBRMaterialProperties& p = m.properties;
            if (std::strcmp(name, "color") == 0)
                p.baseColor = glm::vec4(VectorValue(L, index, name), p.baseColor.a);
            else if (std::strcmp(name, "alpha") == 0)
                p.baseColor.a = NumberValue(L, index, name);
            else if (std::strcmp(name, "metallic") == 0)
                p.metallicFactor = NumberValue(L, index, name);
            else if (std::strcmp(name, "roughness") == 0)
                p.roughnessFactor = NumberValue(L, index, name);
            else if (std::strcmp(name, "emissive") == 0)
                p.emissiveFactor = VectorValue(L, index, name);
            else if (std::strcmp(name, "normalStrength") == 0)
                p.normalMapStrength = NumberValue(L, index, name);
            else if (std::strcmp(name, "occlusionStrength") == 0)
                p.occlusionStrength = NumberValue(L, index, name);
            else if (std::strcmp(name, "clearcoat") == 0)
                p.clearcoat = NumberValue(L, index, name);
            else if (std::strcmp(name, "clearcoatRoughness") == 0)
                p.clearcoatRoughness = NumberValue(L, index, name);
            else if (std::strcmp(name, "doubleSided") == 0)
            {
                if (!lua_isboolean(L, index))
                    Error(L, "doubleSided expects a boolean, got %s", luaL_typename(L, index));
                m.doubleSided = lua_toboolean(L, index) != 0;
            }
            else if (std::strcmp(name, "alphaMode") == 0)
            {
                const int mode = luaL_checkoption(L, index, nullptr, kAlphaModes);
                m.SetAlphaMode(static_cast<AlphaMode>(mode), p.alphaCutoff > 0.0f ? p.alphaCutoff : 0.5f);
            }
            else if (std::strcmp(name, "alphaCutoff") == 0)
                m.SetAlphaMode(AlphaMode::Mask, NumberValue(L, index, name));
            else if (std::strcmp(name, "tiling") == 0 || std::strcmp(name, "offset") == 0)
            {
                const glm::vec3 v = VectorValue(L, index, name);
                for (size_t slot = 0; slot < static_cast<size_t>(MaterialTexture::Count); ++slot)
                {
                    MaterialAsset::TextureTransform transform = m.GetTexture(static_cast<MaterialTexture>(slot)).transform;
                    (name[0] == 't' ? transform.scale : transform.offset) = glm::vec2(v.x, v.y);
                    m.SetTextureTransform(static_cast<MaterialTexture>(slot), transform);
                }
            }
            else
            {
                for (const MapField& map : kMaps)
                {
                    if (std::strcmp(name, map.name) != 0)
                        continue;
                    const MaterialAsset::TextureSlot& slot = m.GetTexture(map.slot);
                    AssetHandle<Texture> texture;
                    if (!lua_isnil(L, index))
                        texture = TextureFor(L, CheckTexture(L, index), map.srgb);
                    m.SetTexture(map.slot, texture, slot.sampler, slot.uvSet);
                    return true;
                }
                return false;
            }
            m.MarkDirty();
            return true;
        }

        // Sets every field the table at `index` names (an error for a name that isn't a field).
        void SetMaterialFields(lua_State* L, MaterialAsset& m, int table)
        {
            table = lua_absindex(L, table);
            lua_pushnil(L);
            while (lua_next(L, table) != 0)
            {
                const char* key = lua_type(L, -2) == LUA_TSTRING ? lua_tostring(L, -2) : nullptr;
                if (!key || !SetMaterialField(L, m, key, lua_gettop(L)))
                    Error(L, "'%s' isn't a material field", key ? key : "?");
                lua_pop(L, 1);
            }
        }

        // material.new(fields): a material of the script's own (white, rough, no textures).
        int MaterialNew(lua_State* L)
        {
            auto material = std::make_shared<MaterialAsset>();
            material->MarkDirty();
            AssetHandle<MaterialAsset> handle = RegisterMaterial(L, material, "Script material");
            if (!lua_isnoneornil(L, 1))
            {
                luaL_checktype(L, 1, LUA_TTABLE);
                SetMaterialFields(L, *material, 1);
            }
            PushMaterial(L, handle);
            return 1;
        }

        int MaterialClone(lua_State* L)
        {
            AssetHandle<MaterialAsset> handle;
            const MaterialAsset& m = CheckMaterial(L, 1, &handle);
            const AssetMetadata* meta = handle.GetMetadata();
            PushMaterial(L, RegisterMaterial(L, m.Clone(), meta ? meta->name : std::string("Script material")));
            return 1;
        }

        int MaterialIndex(lua_State* L)
        {
            AssetHandle<MaterialAsset> handle;
            const MaterialAsset& m = CheckMaterial(L, 1, &handle);
            const PBRMaterialProperties& p = m.properties;
            const char* key = luaL_checkstring(L, 2);
            if (std::strcmp(key, "color") == 0)
                PushVector(L, glm::vec3(p.baseColor));
            else if (std::strcmp(key, "alpha") == 0)
                lua_pushnumber(L, p.baseColor.a);
            else if (std::strcmp(key, "metallic") == 0)
                lua_pushnumber(L, p.metallicFactor);
            else if (std::strcmp(key, "roughness") == 0)
                lua_pushnumber(L, p.roughnessFactor);
            else if (std::strcmp(key, "emissive") == 0)
                PushVector(L, p.emissiveFactor);
            else if (std::strcmp(key, "normalStrength") == 0)
                lua_pushnumber(L, p.normalMapStrength);
            else if (std::strcmp(key, "occlusionStrength") == 0)
                lua_pushnumber(L, p.occlusionStrength);
            else if (std::strcmp(key, "clearcoat") == 0)
                lua_pushnumber(L, p.clearcoat);
            else if (std::strcmp(key, "clearcoatRoughness") == 0)
                lua_pushnumber(L, p.clearcoatRoughness);
            else if (std::strcmp(key, "doubleSided") == 0)
                lua_pushboolean(L, m.doubleSided);
            else if (std::strcmp(key, "alphaMode") == 0)
                lua_pushstring(L, kAlphaModes[static_cast<int>(m.GetAlphaMode())]);
            else if (std::strcmp(key, "alphaCutoff") == 0)
                lua_pushnumber(L, p.alphaCutoff);
            else if (std::strcmp(key, "tiling") == 0 || std::strcmp(key, "offset") == 0)
            {
                const MaterialAsset::TextureTransform& t = m.GetTexture(MaterialTexture::BaseColor).transform;
                const glm::vec2 v = key[0] == 't' ? t.scale : t.offset;
                lua_pushvector(L, v.x, v.y, 0.0f);
            }
            else if (std::strcmp(key, "name") == 0)
            {
                const AssetMetadata* meta = handle.GetMetadata();
                lua_pushstring(L, meta ? meta->name.c_str() : "");
            }
            else if (std::strcmp(key, "Clone") == 0)
                lua_pushcfunction(L, MaterialClone, "Clone");
            else
            {
                for (const MapField& map : kMaps)
                {
                    if (std::strcmp(key, map.name) != 0)
                        continue;
                    const AssetHandle<Texture>& texture = m.GetTexture(map.slot).texture;
                    if (texture.IsValid() && texture.Get())
                        PushTexture(L, texture, map.srgb);
                    else
                        lua_pushnil(L);
                    return 1;
                }
                Error(L, "Material has no member '%s'", key);
            }
            return 1;
        }

        int MaterialNewIndex(lua_State* L)
        {
            MaterialAsset& m = CheckMaterial(L, 1);
            const char* key = luaL_checkstring(L, 2);
            if (!SetMaterialField(L, m, key, 3))
                Error(L, "can't set '%s' on a Material", key);
            return 0;
        }

        int MaterialNamecall(lua_State* L)
        {
            const char* name = lua_namecallatom(L, nullptr);
            if (name && std::strcmp(name, "Clone") == 0)
                return MaterialClone(L);
            Error(L, "Material has no method '%s'", name ? name : "?");
        }

        int MaterialEq(lua_State* L)
        {
            const auto* a = static_cast<const MaterialRef*>(lua_touserdatatagged(L, 1, kMaterialTag));
            const auto* b = static_cast<const MaterialRef*>(lua_touserdatatagged(L, 2, kMaterialTag));
            lua_pushboolean(L, a && b && a->material.GetID() == b->material.GetID());
            return 1;
        }

        int MaterialToString(lua_State* L)
        {
            const auto* ref = static_cast<const MaterialRef*>(lua_touserdatatagged(L, 1, kMaterialTag));
            const AssetMetadata* meta = ref ? ref->material.GetMetadata() : nullptr;
            lua_pushstring(L, ("Material(" + (meta ? meta->name : std::string("?")) + ")").c_str());
            return 1;
        }

        // ---- Libraries ------------------------------------------------------------------------

        void SetFunction(lua_State* L, const char* name, lua_CFunction fn)
        {
            lua_pushcfunction(L, fn, name);
            lua_setfield(L, -2, name);
        }

        void Metatable(lua_State* L, int tag, const char* type, lua_CFunction index, lua_CFunction newIndex, lua_CFunction namecall,
                       lua_CFunction toString, lua_CFunction eq, lua_Destructor destructor)
        {
            lua_newtable(L);
            SetFunction(L, "__index", index);
            if (newIndex)
                SetFunction(L, "__newindex", newIndex);
            if (namecall)
                SetFunction(L, "__namecall", namecall);
            SetFunction(L, "__tostring", toString);
            SetFunction(L, "__eq", eq);
            lua_pushstring(L, type);
            lua_setfield(L, -2, "__type");
            lua_pushstring(L, "locked");
            lua_setfield(L, -2, "__metatable");
            lua_setuserdatametatable(L, tag);
            lua_setuserdatadtor(L, tag, destructor);
        }

        // How many material slots an entity's mesh has: one per submesh material, or as many as it has.
        size_t SlotCount(const MeshComponent& mc)
        {
            size_t count = mc.materials.size();
            if (const IMesh* mesh = mc.mesh.Get().get())
                for (const Submesh& submesh : mesh->submeshes)
                    count = std::max(count, static_cast<size_t>(submesh.materialIndex) + 1);
            return std::max<size_t>(count, 1);
        }

        size_t CheckSlot(lua_State* L, Entity entity, MeshComponent& mc, int index)
        {
            const int slot = luaL_optinteger(L, index, 1);
            const size_t count = SlotCount(mc);
            if (slot < 1 || static_cast<size_t>(slot) > count)
                Error(L, "%s has %d material slot%s, counted from 1 (asked for %d)", entity.GetName(), static_cast<int>(count), count == 1 ? "" : "s",
                      slot);
            if (mc.materials.size() < count)
                mc.materials.resize(count);
            return static_cast<size_t>(slot - 1);
        }
    }

    // entity:SetMesh(mesh, materials): draws the mesh (a Mesh Renderer, casting shadows), with these
    // materials or the ones it had.
    int EntitySetMesh(lua_State* L)
    {
        Entity entity = CheckEntity(L, 1);
        const ScriptMesh& mesh = CheckMesh(L, 2);
        std::vector<AssetHandle<MaterialAsset>> materials;
        const bool given = !lua_isnoneornil(L, 3);
        if (given)
        {
            luaL_checktype(L, 3, LUA_TTABLE);
            const int n = lua_objlen(L, 3);
            for (int i = 1; i <= n; ++i)
            {
                lua_rawgeti(L, 3, i);
                AssetHandle<MaterialAsset> handle;
                CheckMaterial(L, -1, &handle);
                materials.push_back(handle);
                lua_pop(L, 1);
            }
        }
        if (MeshComponent* existing = entity.Get<MeshComponent>())
        {
            existing->mesh = mesh.handle;
            if (given)
                existing->materials = std::move(materials);
        }
        else
            entity.Add<MeshComponent>(MeshComponent{ mesh.handle, std::move(materials) }).AddTag<ShadowCasterTag>();
        return 0;
    }

    // entity:GetMesh(): the mesh it draws (its own, or one a script made), or nil.
    int EntityGetMesh(lua_State* L)
    {
        const Entity entity = CheckEntity(L, 1);
        const MeshComponent* mc = entity.TryGet<MeshComponent>();
        if (!mc || !mc->mesh.Get())
        {
            lua_pushnil(L);
            return 1;
        }
        Context& context = GetContext(L);
        if (const auto found = context.scriptMeshes.find(mc->mesh.Get().get()); found != context.scriptMeshes.end())
        {
            PushMesh(L, found->second);
            return 1;
        }
        auto mesh = std::make_shared<ScriptMesh>();
        mesh->handle = mc->mesh;
        mesh->vertexCount = mc->mesh.Get()->VertexCount();
        PushMesh(L, std::move(mesh));
        return 1;
    }

    // entity:GetMaterial(slot): the material in a slot of its Mesh Renderer (from 1), the entity's
    // own copy of it the first time, so changing it changes this entity only.
    int EntityGetMaterial(lua_State* L)
    {
        Entity entity = CheckEntity(L, 1);
        MeshComponent* mc = entity.Get<MeshComponent>();
        if (!mc)
        {
            lua_pushnil(L);
            return 1;
        }
        AssetHandle<MaterialAsset>& current = mc->materials[CheckSlot(L, entity, *mc, 2)];
        Context& context = GetContext(L);
        if (!current.Get() || context.scriptMaterials.count(current.GetID()) == 0)
        {
            const AssetHandle<MaterialAsset> base = current.Get() ? current : BuiltinAssets::DefaultMaterial(*context.assets);
            const AssetMetadata* meta = base.GetMetadata();
            current = RegisterMaterial(L, base.Get()->Clone(), (meta ? meta->name : std::string("Material")) + " (" + entity.GetName() + ")");
        }
        PushMaterial(L, current);
        return 1;
    }

    // entity:SetMaterial(material, slot): puts a material in a slot of its Mesh Renderer.
    int EntitySetMaterial(lua_State* L)
    {
        Entity entity = CheckEntity(L, 1);
        AssetHandle<MaterialAsset> handle;
        CheckMaterial(L, 2, &handle);
        MeshComponent* mc = entity.Get<MeshComponent>();
        if (!mc)
            Error(L, "%s has no Mesh Renderer: give it a mesh first (entity:SetMesh)", entity.GetName());
        mc->materials[CheckSlot(L, entity, *mc, 3)] = handle;
        return 0;
    }

    void OpenAssets(lua_State* L)
    {
        Metatable(L, kMeshTag, "Mesh", MeshIndex, nullptr, MeshNamecall, MeshToString, MeshEq, DestroyUserdata<MeshRef>);
        Metatable(L, kMaterialTag, "Material", MaterialIndex, MaterialNewIndex, MaterialNamecall, MaterialToString, MaterialEq,
                  DestroyUserdata<MaterialRef>);
        Metatable(L, kTextureTag, "Texture", TextureIndex, nullptr, nullptr, TextureToString, TextureEq, DestroyUserdata<TextureRef>);

        lua_newtable(L);
        SetFunction(L, "new", MeshNew);
        lua_setglobal(L, "mesh");
        lua_newtable(L);
        SetFunction(L, "new", MaterialNew);
        lua_setglobal(L, "material");
        lua_newtable(L);
        SetFunction(L, "load", TextureLoad);
        lua_setglobal(L, "texture");
    }
}
