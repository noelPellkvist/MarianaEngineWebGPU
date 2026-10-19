#include <Mariana/Animation/Skeleton.hpp>

#include <utility>

void Skeleton::AddBone(Bone bone)
{
    const int index = static_cast<int>(m_Bones.size());
    // The first bone with a name keeps it (glTF names needn't be unique).
    m_ByName.emplace(std::string(BaseName(bone.name)), index);
    if (bone.node != ~0u)
        m_ByNode.emplace(bone.node, index);
    m_Bones.push_back(std::move(bone));
}

int Skeleton::FindBone(std::string_view name) const
{
    const auto it = m_ByName.find(std::string(BaseName(name)));
    return it == m_ByName.end() ? -1 : it->second;
}

int Skeleton::FindBoneByNode(uint32_t node) const
{
    const auto it = m_ByNode.find(node);
    return it == m_ByNode.end() ? -1 : it->second;
}

bool Skeleton::IsDescendant(int bone, int ancestor) const
{
    while (bone >= 0)
    {
        if (bone == ancestor)
            return true;
        bone = m_Bones[bone].parent;
    }
    return false;
}

std::string_view Skeleton::BaseName(std::string_view name)
{
    const size_t colon = name.rfind(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}
